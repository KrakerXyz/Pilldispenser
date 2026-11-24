#include "lvgl.h"
#include "ui.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Only Arduino/platform services are substituted. LVGL, screen objects,
// callbacks, styles, confirmation logic, and command reservation are real.
class String {
  std::string value;
public:
  String() = default;
  String(const char *s) : value(s ? s : "") {}
  String(const std::string &s) : value(s) {}
  String(int i) : value(std::to_string(i)) {}
  const char *c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  void trim() {
    const auto start=value.find_first_not_of(" \t\r\n");
    value=start==std::string::npos ? "" : value.substr(start,value.find_last_not_of(" \t\r\n")-start+1);
  }
  String &operator+=(const String &s) { value+=s.value; return *this; }
  friend String operator+(const String &a,const String &b) { return a.value+b.value; }
  friend bool operator==(const String &a,const String &b) { return a.value==b.value; }
  friend bool operator!=(const String &a,const String &b) { return !(a==b); }
};
using portMUX_TYPE=std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(m) (m)->lock()
#define portEXIT_CRITICAL(m) (m)->unlock()
constexpr int LOW=0,HIGH=1;
static void require(bool condition,const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}
struct PreferencesStub {
  std::map<std::string,bool> values;
  std::map<std::string,int> numbers;
  void begin(const char *,bool) {}
  void putBool(const char *key,bool value) { values[key]=value; }
  void putInt(const char *key,int value) { numbers[key]=value; }
  int getInt(const char *key,int fallback) { auto it=numbers.find(key);return it==numbers.end()?fallback:it->second; }
  void end() {}
} preferences;
struct PixelsStub {
  uint32_t Color(unsigned r,unsigned g,unsigned b) { return (r<<16)|(g<<8)|b; }
  void setPixelColor(int,uint32_t) {} void setBrightness(int) {} void show() {}
} pixels;
struct EspStub { void restart() { throw std::runtime_error("Unexpected restart in UI test"); } } ESP;
uint32_t millis() { return lv_tick_get(); }
int installedTrays=10, selectedtray=0, nextaction=0, flashing=3, traytodispense=-1, displacesteps=0;
bool systemloaded=true, enabledpasscode=false, clockSS=false, solidLED=false, alertinprogress=false;
bool trayEnabled[11]={}, traytriggered[11]={}, trayAlertEna[11]={}, traydisptoday[11]={}, traydismtoday[11]={}, trayfastflash[11]={}, alreadyTriggered[11]={};
int trayColor[11]={},trayHours[11]={},trayMin[11]={},resetHours=0,resetMin=0;
String trayNames[11],passcode="1234";
uint32_t currentpixelcolor=0;
lv_obj_t *ui_TrayIMG[11]={}, *ui_TrayLBL[11]={}, *ui_TrayCheckIMG[11]={};
lv_timer_t *inactivity_timer=nullptr;
static int loadedTray=0, savedTray=0, settingsLoads=0, pin21=1;
static unsigned motorWrites=0;
void digitalWrite(int pin,int value) { if (pin==21) pin21=value; }
void setServoPulse(int,int) { ++motorWrites; }
void detachServo(int) { ++motorWrites; }
void showLED(uint32_t);
void reconcileCurrentDay();
static void reset_triggered_timer(lv_timer_t *);
lv_color_t index_to_color(int);
void saveSettings() {}
void loadSettings() {
  ++settingsLoads;
  lv_dropdown_set_selected(ui_HowManyTrayDROP,installedTrays-1);
  lv_dropdown_set_selected(ui_DayHourDROP,0);lv_dropdown_set_selected(ui_DayMinutesDROP,0);
  lv_obj_clear_state(ui_DSTCHK,LV_STATE_CHECKED);
  lv_slider_set_value(ui_UTCSlider,-4,LV_ANIM_OFF);lv_label_set_text(ui_UTCLBL,"UTC Offset: -4");
  lv_obj_add_state(ui_ScreensaverCHK1,LV_STATE_CHECKED);
}
void clearWifiPreferences() {}
void clearTrayPreferences(int) {}
void saveTraySettings(int tray) { savedTray=tray; }
void loadTraySettings(int tray) {
  loadedTray=tray;
  lv_textarea_set_text(ui_TraydescTXT,trayNames[tray].c_str());
  lv_dropdown_set_selected(ui_TrayHourDROP,trayHours[tray]);
  lv_dropdown_set_selected(ui_TrayMinutesDROP,trayMin[tray]/5);
  lv_obj_add_state(ui_TrayAlertCHK,LV_STATE_CHECKED);
  lv_roller_set_selected(ui_ColorRoller,trayColor[tray],LV_ANIM_OFF);
  lv_obj_set_style_bg_color(ui_ColorPanel,index_to_color(trayColor[tray]),LV_PART_MAIN);
}
void placethings();
lv_color_t index_to_color(int);
uint32_t index_to_pixel(int);
#include "AppState.h"
#include "Trays_Stuff.ino"
#include "LVGL_Buttons.ino"
#include "Lock_Screen.ino"
#include "Dispense_Rooutine.ino"
#include "Restart_Control.ino"
#include "Check_Alerts.ino"

static lv_color_t drawing[320*24],frame[320*240];
static lv_disp_draw_buf_t drawBuffer;
static std::filesystem::path frameDir;
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *pixelsIn) {
  for(int y=area->y1;y<=area->y2;++y) for(int x=area->x1;x<=area->x2;++x) {
    if(x>=0&&x<320&&y>=0&&y<240) frame[y*320+x]=*pixelsIn;
    ++pixelsIn;
  }
  lv_disp_flush_ready(driver);
}
static void capture(const std::string &name) {
  for(int i=0;i<3;++i) { lv_tick_inc(33); lv_timer_handler(); }
  lv_obj_update_layout(lv_scr_act()); lv_obj_invalidate(lv_scr_act()); lv_refr_now(nullptr);
  const auto path=frameDir/(name+".ppm");
  FILE *out=std::fopen(path.c_str(),"wb"); require(out,"Cannot open capture");
  std::fprintf(out,"P6\n320 240\n255\n");
  for(auto pixel:frame) {
    lv_color32_t rgb; rgb.full=lv_color_to32(pixel);
    const unsigned char bytes[]={rgb.ch.red,rgb.ch.green,rgb.ch.blue};
    std::fwrite(bytes,1,3,out);
  }
  std::fclose(out);
}
static bool shown(lv_obj_t *object) { return !lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN); }
static std::string textOf(lv_obj_t *object) { return lv_label_get_text(object); }
static void click(lv_obj_t *object) {
  require(shown(object),"Attempted to click a hidden object");
  require(!lv_obj_has_state(object,LV_STATE_DISABLED),"Attempted to click a disabled object");
  lv_event_send(object,LV_EVENT_CLICKED,nullptr);
}
static void assertNoCommand() { require(!controlPending&&!controlBusy&&motorWrites==0,"UI queued or moved a tray unexpectedly"); }
static void assertCommand(int kind,uint16_t mask) {
  require(controlPending&&controlBusy&&controlCommand.kind==kind&&controlCommand.mask==mask,
          "UI queued the wrong command kind/tray mask");
  require(motorWrites==0,"UI moved hardware outside controller processing");
}
static void reset() {
  controlPending=false;controlBusy=false;rebootPending=false;restartScheduled=false;controlCommand={};
  activeControlKind=0;remainingDispenseMask=0;traytodispense=-1;
  installedTrays=10;enabledpasscode=false;selectedtray=0;nextaction=0;flashing=3;motorWrites=0;
  loadedTray=0;savedTray=0;settingsLoads=0;pin21=1;
  for(int i=1;i<=10;++i) {
    trayEnabled[i]=true;trayAlertEna[i]=true;traytriggered[i]=false;
    traydisptoday[i]=false;traydismtoday[i]=false;trayfastflash[i]=false;
    trayColor[i]=i<=3?0:i<=6?3:i<=9?2:5;
    trayHours[i]=i<=3?6:i<=6?12:i<=9?16:20;trayMin[i]=(i<=3||(i>=7&&i<=9))?30:0;
    trayNames[i]=i<=3?"Morning":i<=6?"Lunch":i<=9?"Dinner":"Night";
  }
  closeDispenseConfirmation();
  if(motionOverlay) lv_obj_add_flag(motionOverlay,LV_OBJ_FLAG_HIDDEN);
  lv_textarea_set_text(ui_PassTXT,"");
  lv_scr_load(ui_MainSCR);placethings();refreshTraySelectors();
  lv_label_set_text(ui_ClockLBL,"12:00");
}
static void dueLunch() { traytriggered[4]=traytriggered[5]=traytriggered[6]=true;placethings(); }
static void confirmButtons(bool skip,bool group) {
  require(shown(ui_DispenseConfirmPAN),"Confirmation is hidden");
  require(shown(ui_ConfirmCancelBTN)&&shown(ui_ConfirmSingleBTN),"Missing cancel/single button");
  require(shown(ui_ConfirmSkipBTN)==skip&&shown(ui_ConfirmGroupBTN)==group,"Wrong group action visibility");
}
static void initialize() {
  lv_init();lv_disp_draw_buf_init(&drawBuffer,drawing,nullptr,320*24);
  static lv_disp_drv_t driver;lv_disp_drv_init(&driver);
  driver.hor_res=320;driver.ver_res=240;driver.flush_cb=flush;driver.draw_buf=&drawBuffer;
  lv_disp_drv_register(&driver);ui_init();
  lv_obj_t *buttons[]={nullptr,ui_TrayIMG1,ui_TrayIMG2,ui_TrayIMG3,ui_TrayIMG4,ui_TrayIMG5,ui_TrayIMG6,ui_TrayIMG7,ui_TrayIMG8,ui_TrayIMG9,ui_TrayIMG10};
  lv_obj_t *names[]={nullptr,ui_TrayLBL1,ui_TrayLBL2,ui_TrayLBL3,ui_TrayLBL4,ui_TrayLBL5,ui_TrayLBL6,ui_TrayLBL7,ui_TrayLBL8,ui_TrayLBL9,ui_TrayLBL10};
  lv_obj_t *checks[]={nullptr,ui_TrayCheckIMG1,ui_TrayCheckIMG2,ui_TrayCheckIMG3,ui_TrayCheckIMG4,ui_TrayCheckIMG5,ui_TrayCheckIMG6,ui_TrayCheckIMG7,ui_TrayCheckIMG8,ui_TrayCheckIMG9,ui_TrayCheckIMG10};
  std::copy(std::begin(buttons),std::end(buttons),ui_TrayIMG);
  std::copy(std::begin(names),std::end(names),ui_TrayLBL);
  std::copy(std::begin(checks),std::end(checks),ui_TrayCheckIMG);
  bindFlowEvents();
  // These bindings live in main setup(), not bindFlowEvents().
  lv_obj_add_event_cb(ui_PassKey,passkey_event_handler,LV_EVENT_ALL,nullptr);
  lv_obj_add_event_cb(ui_SettingBackBTN,SettingBackBTN_event_handler,LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(ui_DisplaceBTN,DisplaceBTN_event_handler,LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(ui_TrayCfgSaveBTN,TrayCfgSaveBTN_event_handler,LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(ui_TrayCfgClearBTN,TrayCfgClearBTN_event_handler,LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(ui_ColorRoller,roller_event_handler,LV_EVENT_VALUE_CHANGED,nullptr);
}
int main(int argc,char **argv) {
  try {
    require(argc==2,"Expected output directory argument");frameDir=argv[1];std::filesystem::create_directories(frameDir);
    initialize();
    const std::vector<std::pair<const char *,std::function<void()>>> cases={
      {"manual confirmation and cancel",[]{capture("12-home-plain");click(ui_TrayIMG[2]);confirmButtons(false,false);require(confirmationTray==2&&confirmationGroup==0,"Wrong manual tray");require(textOf(ui_ConfirmSingleLBL)=="Dispense 2","Wrong single label");capture("01-manual-confirmation");click(ui_ConfirmCancelBTN);require(!shown(ui_DispenseConfirmPAN),"Cancel did not close");assertNoCommand();}},
      {"ready color group and single",[]{dueLunch();capture("13-home-ready");click(ui_TrayIMG[5]);confirmButtons(true,true);require(confirmationGroup==(trayBit(4)|trayBit(5)|trayBit(6)),"Wrong due group");require(textOf(ui_ConfirmGroupLBL)=="Dispense Lunch"&&textOf(ui_ConfirmSkipLBL)=="Skip Lunch","Wrong group label");capture("02-ready-group");click(ui_ConfirmSingleBTN);assertCommand(CONTROL_DISPENSE,trayBit(5));require(traytriggered[4]&&traytriggered[6],"Single action consumed peers");}},
      {"group command capture",[]{dueLunch();click(ui_TrayIMG[4]);click(ui_ConfirmGroupBTN);assertCommand(CONTROL_GROUP,trayBit(4)|trayBit(5)|trayBit(6));}},
      {"skip command capture",[]{dueLunch();click(ui_TrayIMG[6]);click(ui_ConfirmSkipBTN);assertCommand(CONTROL_SKIP,trayBit(4)|trayBit(5)|trayBit(6));}},
      {"outside due group stays manual",[]{dueLunch();click(ui_TrayIMG[10]);confirmButtons(false,false);require(confirmationGroup==0,"Unrelated ready group leaked into manual dialog");capture("03-outside-group");click(ui_ConfirmSingleBTN);assertCommand(CONTROL_DISPENSE,trayBit(10));}},
      {"one ready tray",[]{traytriggered[10]=true;click(ui_TrayIMG[10]);confirmButtons(true,false);require(textOf(ui_ConfirmSkipLBL)=="Skip Night","Single due skip label missing");capture("04-single-ready");}},
      {"repeat warning",[]{traydisptoday[1]=true;click(ui_TrayIMG[1]);confirmButtons(false,false);require(textOf(ui_ConfirmHeadingLBL)=="Already dispensed today"&&textOf(ui_ConfirmSingleLBL)=="Dispense again","Repeat warning missing");capture("05-repeat-warning");click(ui_ConfirmSingleBTN);assertCommand(CONTROL_DISPENSE,trayBit(1));}},
      {"stale due group reopens",[]{dueLunch();click(ui_TrayIMG[4]);traytriggered[6]=false;click(ui_ConfirmGroupBTN);assertNoCommand();require(shown(ui_DispenseConfirmPAN)&&confirmationGroup==(trayBit(4)|trayBit(5)),"Stale group was not refreshed");click(ui_ConfirmGroupBTN);assertCommand(CONTROL_GROUP,trayBit(4)|trayBit(5));}},
      {"stale repeat state reopens",[]{click(ui_TrayIMG[2]);traydisptoday[2]=true;click(ui_ConfirmSingleBTN);assertNoCommand();require(textOf(ui_ConfirmHeadingLBL)=="Already dispensed today","Stale repeat did not warn");}},
      {"passcode correct",[]{enabledpasscode=true;click(ui_TrayIMG[3]);require(lv_scr_act()==ui_LockSCR&&nextaction==1,"Tray did not require passcode");capture("06-passcode");lv_textarea_set_text(ui_PassTXT,"1234");lv_event_send(ui_PassKey,LV_EVENT_READY,nullptr);confirmButtons(false,false);require(confirmationTray==3&&nextaction==0&&std::strlen(lv_textarea_get_text(ui_PassTXT))==0,"Passcode route/cleanup wrong");assertNoCommand();}},
      {"passcode wrong and cancel",[]{enabledpasscode=true;click(ui_TrayIMG[1]);lv_textarea_set_text(ui_PassTXT,"9999");lv_event_send(ui_PassKey,LV_EVENT_READY,nullptr);require(lv_scr_act()==ui_MainSCR&&!shown(ui_DispenseConfirmPAN),"Wrong passcode reached confirmation");assertNoCommand();click(ui_HomeConfigBTN);lv_textarea_set_text(ui_PassTXT,"1234");lv_event_send(ui_PassKey,LV_EVENT_CANCEL,nullptr);require(lv_scr_act()==ui_MainSCR&&!nextaction,"Cancel reached Config");assertNoCommand();}},
      {"protected Config",[]{enabledpasscode=true;click(ui_HomeConfigBTN);require(lv_scr_act()==ui_LockSCR&&nextaction==3,"Config did not require passcode");lv_textarea_set_text(ui_PassTXT,"1234");lv_event_send(ui_PassKey,LV_EVENT_READY,nullptr);require(lv_scr_act()==ui_ConfigSCR,"Correct passcode did not open Config");}},
      {"Config navigation and tray selection",[]{click(ui_HomeConfigBTN);require(lv_scr_act()==ui_ConfigSCR,"Config missing");capture("07-config");click(ui_ConfigTrayBTN);require(lv_scr_act()==ui_TraySelectSCR,"Tray selector missing");capture("08-tray-selection");click(ui_TraySelectBTN[7]);require(lv_scr_act()==ui_TrayConfigSCR&&loadedTray==7&&selectedtray==7,"Tray selection wrong");require(textOf(ui_TrayConfigTitleLBL)=="Tray 7","Tray heading wrong");capture("14-tray-config");click(ui_TrayCfgBackBTN);require(lv_scr_act()==ui_TraySelectSCR,"Tray back route wrong");click(ui_TraySelectBackBTN);click(ui_ConfigSystemBTN);require(lv_scr_act()==ui_SettingSCR&&settingsLoads==1,"System route wrong");capture("15-system");click(ui_SettingBackBTN1);click(ui_ConfigDisplayBTN);require(lv_scr_act()==ui_DisplaySCR&&settingsLoads==2,"Display route wrong");capture("16-display");click(ui_DisplayBackBTN);click(ui_ConfigWifiBTN);require(lv_scr_act()==ui_WifiSCR,"Wi-Fi route wrong");lv_dropdown_set_options(ui_SSIDDropDown,"Home Wi-Fi\nGuest Wi-Fi");lv_textarea_set_text(ui_PassTxtBox,"");capture("17-wifi");assertNoCommand();}},
      {"Fix selects tray immediately",[]{click(ui_HomeConfigBTN);click(ui_ConfigFixBTN);require(lv_scr_act()==ui_TrayFixSCR,"Fix screen missing");capture("09-tray-alignment");click(ui_TrayFixBTN[8]);assertCommand(CONTROL_ALIGN,trayBit(8));require(!shown(ui_DispenseConfirmPAN),"Fix unexpectedly used dispense confirmation");}},
      {"actual status styles",[]{traydisptoday[1]=true;traydismtoday[7]=true;traytriggered[4]=true;flashing=0;placethings();require(lv_obj_get_style_border_width(ui_TrayIMG[1],0)==4,"Dispensed border missing");require(lv_obj_get_style_border_color(ui_TrayIMG[1],0).full==lv_color_hex(0x24E36B).full,"Dispensed border wrong color");require(shown(ui_TrayCheckIMG[1])&&textOf(ui_TrayCheckIMG[1])==LV_SYMBOL_OK,"Dispensed status missing");require(shown(ui_TrayCheckIMG[7])&&textOf(ui_TrayCheckIMG[7])==LV_SYMBOL_CLOSE,"Skipped status missing");require(lv_obj_get_style_bg_opa(ui_TrayIMG[4],0)==LV_OPA_50,"Ready flash dim phase missing");capture("10-runtime-status");}},
      {"busy controls do not dispatch",[]{controlBusy=true;click(ui_TrayIMG[1]);click(ui_HomeConfigBTN);require(lv_scr_act()==ui_MainSCR&&!shown(ui_DispenseConfirmPAN)&&!controlPending,"Busy input changed action");}},
      {"motion overlay",[]{activeControlKind=CONTROL_DISPENSE;traytodispense=5;setMotionOverlay(true);require(shown(motionOverlay)&&textOf(motionLabel).find("Dispensing tray 5")!=std::string::npos,"Motion overlay wrong");capture("11-motion-overlay");setMotionOverlay(false);}}
    };
    for(const auto &test:cases) { reset();test.second();std::cout<<"PASS "<<test.first<<'\n'; }
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    std::cout<<cases.size()<<" UI runtime cases passed; LVGL free="<<memory.free_size<<", largest="<<memory.free_biggest_size<<" bytes\n";
    return 0;
  } catch(const std::exception &error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
