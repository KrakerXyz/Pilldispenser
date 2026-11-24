#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// These substitutes record effects; the controller itself is included unchanged.
struct lv_obj_t { unsigned flags = 0; int selected = -1; };
struct lv_timer_t;
using lv_timer_cb_t = void (*)(lv_timer_t *);
struct lv_timer_t {
  lv_timer_cb_t callback;
  unsigned period;
  unsigned due;
  bool alive;
};
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()
constexpr unsigned LV_OBJ_FLAG_HIDDEN = 1;

static void require(bool result, const std::string &message) {
  if (!result) throw std::runtime_error(message);
}
static unsigned nowMs;
uint32_t millis() { return nowMs; }
struct EspStub {
  unsigned restarts = 0;
  void restart() { ++restarts; }
} ESP;
static std::vector<std::unique_ptr<lv_timer_t>> timers;
static unsigned inactivityResets;
lv_timer_t *lv_timer_create(lv_timer_cb_t callback, unsigned period, void *) {
  timers.emplace_back(new lv_timer_t{callback, period, nowMs + period, true});
  return timers.back().get();
}
void lv_timer_del(lv_timer_t *timer) { timer->alive = false; }
void lv_timer_reset(lv_timer_t *) { ++inactivityResets; }
static bool pumpOne() {
  lv_timer_t *next = nullptr;
  for (const auto &timer : timers)
    if (timer->alive && (!next || timer->due < next->due)) next = timer.get();
  if (!next) return false;
  nowMs = next->due;
  next->callback(next);
  if (next->alive) next->due = nowMs + next->period;
  return true;
}
static void drainTimers() {
  unsigned executions = 0;
  while (pumpOne()) require(++executions < 1000, "Controller failed to retire its timers");
}

struct Motion { int channel; int angle; unsigned at; };
static std::vector<Motion> motion;
void setServoPulse(int channel, int angle) { motion.push_back({channel, angle, nowMs}); }
void detachServo(int channel) { motion.push_back({channel, -1, nowMs}); }
struct PreferencesStub {
  bool open = false;
  unsigned writes = 0;
  std::map<std::string, bool> values;
  void begin(const char *name, bool readOnly) {
    require(!open && !readOnly && std::string(name) == "traySettings", "Wrong persistence transaction");
    open = true;
  }
  void putBool(const char *key, bool value) {
    require(open, "Status persisted outside a Preferences transaction");
    values[key] = value;
    ++writes;
  }
  void end() { require(open, "Unbalanced Preferences end"); open = false; }
} preferences;
struct PixelsStub {
  uint32_t Color(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }
} pixels;
static uint32_t displayedLed;
static bool overlayVisible;
static unsigned confirmationCloses;
static unsigned layoutRefreshes;
static lv_obj_t inprogressObject, displacementObject, mainObject;
lv_obj_t *ui_InprogressPAN = &inprogressObject;
lv_obj_t *ui_DisplaceTrayDROP = &displacementObject;
lv_obj_t *ui_MainSCR = &mainObject;
static lv_obj_t *loadedScreen;
void showLED(uint32_t color) { displayedLed = color; }
uint32_t index_to_pixel(int color) { return static_cast<uint32_t>(color + 1); }
void setMotionOverlay(bool visible) { overlayVisible = visible; }
void closeDispenseConfirmation() { ++confirmationCloses; }
void placethings() { ++layoutRefreshes; }
void lv_obj_add_flag(lv_obj_t *object, unsigned flag) { object->flags |= flag; }
void lv_dropdown_set_selected(lv_obj_t *object, int selected) { object->selected = selected; }
void lv_scr_load(lv_obj_t *screen) { loadedScreen = screen; }

int installedTrays = 10;
bool systemloaded = true;
bool traytriggered[11] = {};
int trayColor[11] = {};
bool traydisptoday[11] = {};
bool traydismtoday[11] = {};
bool trayfastflash[11] = {};
bool trayAlertEna[11] = {};
int displacesteps = 0;
int traytodispense = -1;
uint32_t currentpixelcolor = 0;
bool solidLED = false;
static lv_timer_t inactivityStub = {nullptr, 0, 0, false};
lv_timer_t *inactivity_timer = &inactivityStub;
static unsigned dayReconciliations = 0;
void reconcileCurrentDay();

#include "AppState.h"
#include "Dispense_Rooutine.ino"
#include "Restart_Control.ino"
void reconcileCurrentDay() {
  require(controlPending && activeControlKind == 0 && preferences.writes == 0,
          "Logical day must reconcile before the queued command records status");
  ++dayReconciliations;
}

static void resetFixture() {
  timers.clear(); motion.clear(); nowMs = 0; inactivityResets = 0;
  preferences = PreferencesStub{};
  installedTrays = 10; systemloaded = true;
  std::fill(std::begin(traytriggered), std::end(traytriggered), false);
  std::fill(std::begin(trayColor), std::end(trayColor), 0);
  std::fill(std::begin(traydisptoday), std::end(traydisptoday), false);
  std::fill(std::begin(traydismtoday), std::end(traydismtoday), false);
  std::fill(std::begin(trayfastflash), std::end(trayfastflash), false);
  std::fill(std::begin(trayAlertEna), std::end(trayAlertEna), true);
  controlBusy = false; controlPending = false; controlCommand = {};
  rebootPending = false; restartScheduled = false; restartAtMillis = 0; ESP.restarts = 0;
  activeControlKind = 0; remainingDispenseMask = 0;
  displacesteps = 0; traytodispense = -1; currentpixelcolor = 0;
  solidLED = false; displayedLed = 99; overlayVisible = false;
  confirmationCloses = 0; layoutRefreshes = 0; loadedScreen = nullptr;
  inprogressObject = {}; displacementObject = {}; mainObject = {};
  inactivity_timer = &inactivityStub;
  dayReconciliations = 0;
}
static void assertFinished() {
  require(!controlBusy && !controlPending, "Controller reservation was not released");
  require(!overlayVisible && remainingDispenseMask == 0 && traytodispense == -1,
          "Motion state was not cleared");
  require(!preferences.open, "Preferences transaction remained open");
  require(displayedLed == 0 && !solidLED, "LED was not cleared");
  require(inactivityResets == 1 && layoutRefreshes == 1, "Completion hooks did not run once");
  require(dayReconciliations == 1, "Queued command did not reconcile the logical day once");
  require(!pumpOne(), "Live motion timer remained after completion");
}
static void assertCycle(int tray, size_t offset = 0, unsigned startsAt = 250) {
  const int angles[] = {180, 70, 180, 70, -1};
  const unsigned times[] = {0, 300, 800, 1100, 1600};
  require(motion.size() >= offset + 5, "Incomplete dispense motion");
  for (size_t i = 0; i < 5; ++i) {
    require(motion[offset + i].channel == tray - 1, "Motor command targeted the wrong tray");
    require(motion[offset + i].angle == angles[i], "Motor command sequence changed");
    require(motion[offset + i].at == startsAt + times[i], "Motor timing changed");
  }
}
static void assertPersisted(int tray, bool dispensed, bool skipped) {
  require(preferences.values.at("traydisptoday" + std::to_string(tray)) == dispensed,
          "Wrong persisted dispensed status");
  require(preferences.values.at("traydismtoday" + std::to_string(tray)) == skipped,
          "Wrong persisted skipped status");
}

static void singleLeavesDuePeers() {
  traytriggered[1] = traytriggered[2] = traytriggered[3] = true;
  require(requestControl(CONTROL_DISPENSE, trayBit(2), 0), "Single dispense rejected");
  processControlCommand();
  require(motion.empty(), "Controller moved before its first timer");
  require(traydisptoday[2] && !traytriggered[2], "Single tray status was not reserved before motion");
  assertPersisted(2, true, false);
  drainTimers();
  require(motion.size() == 5, "Single dispense moved more than one tray");
  assertCycle(2);
  require(traytriggered[1] && traytriggered[3] && !traydisptoday[1] && !traydisptoday[3],
          "Single dispense consumed another due tray");
  require(preferences.writes == 2, "Single dispense persisted another tray");
  assertFinished();
}
static void groupUsesOnlyColorSnapshot() {
  traytriggered[1] = traytriggered[2] = traytriggered[3] = true;
  trayColor[1] = trayColor[3] = trayColor[4] = 2; trayColor[2] = 3;
  require(dueGroupMask(2) == (trayBit(1) | trayBit(3)), "Wrong color group");
  require(requestFirstDueGroup(false), "Due group rejected");
  traytriggered[4] = true; // It became due after the accepted command's snapshot.
  processControlCommand();
  processControlCommand(); // Consuming the queued command twice must be harmless.
  drainTimers();
  require(motion.size() == 10, "Group did not dispense exactly its two captured trays");
  assertCycle(1); assertCycle(3, 5, 2100);
  require(traytriggered[2] && traytriggered[4], "Group consumed another color or a newly due tray");
  require(!traydisptoday[2] && !traydisptoday[4] && preferences.writes == 4,
          "Group persisted a tray outside its snapshot");
  assertPersisted(1, true, false); assertPersisted(3, true, false);
  assertFinished();
}
static void groupDropsNoLongerDueTrays() {
  traytriggered[1] = traytriggered[3] = true;
  require(requestFirstDueGroup(false), "Group rejected");
  traytriggered[3] = false;
  processControlCommand(); drainTimers();
  require(motion.size() == 5 && !traydisptoday[3], "Resolved tray was still dispensed");
  assertCycle(1); assertFinished();
}
static void emptyResolvedGroupDoesNotMove() {
  traytriggered[1] = true;
  require(requestFirstDueGroup(false), "Group rejected");
  traytriggered[1] = false;
  processControlCommand(); drainTimers();
  require(motion.empty() && preferences.writes == 0, "Resolved group produced effects");
  assertFinished();
}
static void skipHasNoMotorEffects() {
  traytriggered[1] = traytriggered[2] = traytriggered[3] = true;
  trayColor[2] = 3;
  require(requestFirstDueGroup(true), "Skip rejected");
  processControlCommand(); drainTimers();
  require(motion.empty(), "Skip actuated a motor");
  require(traydismtoday[1] && traydismtoday[3] && !traytriggered[1] && !traytriggered[3],
          "Skip did not mark its captured group");
  require(traytriggered[2] && !traydismtoday[2], "Skip consumed another color");
  require(!traydisptoday[1] && !traydisptoday[3] && preferences.writes == 4, "Skip persistence mismatch");
  assertPersisted(1, false, true); assertPersisted(3, false, true);
  assertFinished();
}
static void manualOnlyStillRecordsDailyDispense() {
  trayAlertEna[8] = false; traydismtoday[8] = true;
  require(requestControl(CONTROL_DISPENSE, trayBit(8), 0), "Manual-only dispense rejected");
  processControlCommand(); drainTimers();
  require(traydisptoday[8] && !traydismtoday[8], "Manual-only daily status was not recorded");
  assertPersisted(8, true, false); assertCycle(8); assertFinished();
}
static void busyRejectsDuplicateAndOverlappingCommands() {
  require(requestControl(CONTROL_DISPENSE, trayBit(1), 0), "Initial command rejected");
  require(!requestControl(CONTROL_DISPENSE, trayBit(1), 0), "Duplicate queued command accepted");
  require(!requestControl(CONTROL_ALIGN, trayBit(2), 0), "Overlapping queued action accepted");
  processControlCommand();
  require(pumpOne(), "First motor timer missing");
  require(!requestControl(CONTROL_ADVANCE, trayBit(2), 2), "Command accepted during motor movement");
  drainTimers(); require(motion.size() == 5, "Busy rejection still scheduled another motion");
  assertFinished();
}
static void simultaneousRequestsReserveExactlyOnce() {
  std::atomic<unsigned> ready{0}, accepted{0}; std::atomic<bool> go{false};
  std::vector<std::thread> workers;
  for (unsigned i = 0; i < 12; ++i) workers.emplace_back([&, i] {
    ++ready; while (!go.load()) std::this_thread::yield();
    if (requestControl(CONTROL_DISPENSE, trayBit(1 + i % 2), 0)) ++accepted;
  });
  while (ready.load() != workers.size()) std::this_thread::yield();
  go = true; for (auto &worker : workers) worker.join();
  require(accepted == 1, "Concurrent requests reserved more than one action");
  processControlCommand(); drainTimers();
  require(motion.size() == 5, "Concurrent requests created overlapping motions");
  assertFinished();
}
static void alignmentIsExactlyOneHalfStepWithoutStatusChanges() {
  traytriggered[5] = true; traydismtoday[5] = true;
  require(requestControl(CONTROL_ALIGN, trayBit(5), 0), "Alignment rejected");
  processControlCommand(); drainTimers();
  require(motion.size() == 3, "Alignment was not exactly a half-step");
  const int angles[] = {180, 70, -1}; const unsigned times[] = {250, 550, 1050};
  for (size_t i = 0; i < 3; ++i)
    require(motion[i].channel == 4 && motion[i].angle == angles[i] && motion[i].at == times[i],
            "Alignment channel/sequence/timing mismatch");
  require(traytriggered[5] && traydismtoday[5] && !traydisptoday[5] && preferences.writes == 0,
          "Alignment modified medication status");
  assertFinished();
}
static void advancementCountsCyclesWithoutStatusChanges() {
  for (int count : {1, 3, 30}) {
    resetFixture(); traytriggered[6] = true; traydisptoday[6] = true;
    require(requestControl(CONTROL_ADVANCE, trayBit(6), count), "Valid advancement rejected");
    processControlCommand(); drainTimers();
    require(motion.size() == size_t(4 * count + 1), "Advancement cycle count mismatch");
    const int angles[] = {180, 70, 180, 70}; const unsigned offsets[] = {0, 300, 800, 1100};
    for (int cycle = 0; cycle < count; ++cycle) for (int step = 0; step < 4; ++step) {
      const Motion &m = motion[4 * cycle + step];
      require(m.channel == 5 && m.angle == angles[step] && m.at == 250U + 1600U * cycle + offsets[step],
              "Advancement pulse sequence/timing mismatch");
    }
    require(motion.back().channel == 5 && motion.back().angle == -1 &&
            motion.back().at == 250U + 1600U * count, "Advancement did not finish detached");
    require(traytriggered[6] && traydisptoday[6] && !traydismtoday[6] && preferences.writes == 0,
            "Advancement changed medication status");
    require(displacementObject.selected == 0, "Advancement position picker was not reset");
    assertFinished();
  }
}
static void invalidCommandsNeverMove() {
  require(trayBit(0) == 0 && trayBit(-1) == 0 && trayBit(11) == 0, "Invalid tray index mapped to a motor");
  require(!requestFirstDueGroup(false) && !requestFirstDueGroup(true), "Empty due group accepted");
  require(!requestControl(CONTROL_DISPENSE, 0, 0), "Empty tray mask accepted");
  require(!requestControl(CONTROL_DISPENSE, 1U << 10, 0), "Tray11 accepted");
  require(!requestControl(CONTROL_DISPENSE, trayBit(1) | trayBit(2), 0),
          "Single-dispense command accepted multiple trays");
  installedTrays = 5;
  require(!requestControl(CONTROL_DISPENSE, trayBit(6), 0), "Uninstalled tray accepted");
  require(!requestControl(CONTROL_GROUP, trayBit(1) | trayBit(6), 0),
          "Partially invalid tray mask was clipped instead of rejected");
  require(!requestControl(CONTROL_SKIP, trayBit(1) | (1U << 10), 0),
          "Skip accepted an out-of-range bit alongside a valid tray");
  require(!requestControl(CONTROL_ALIGN, trayBit(1) | trayBit(2), 0), "Multiple alignment trays accepted");
  require(!requestControl(CONTROL_ADVANCE, trayBit(1) | trayBit(2), 1), "Multiple advancement trays accepted");
  for (int count : {-1, 0, 31})
    require(!requestControl(CONTROL_ADVANCE, trayBit(1), count), "Invalid step count accepted");
  require(!requestControl(0, trayBit(1), 0) && !requestControl(6, trayBit(1), 0), "Unknown command accepted");
  systemloaded = false;
  require(!requestControl(CONTROL_DISPENSE, trayBit(1), 0), "Motion accepted before boot completed");
  processControlCommand(); drainTimers();
  require(motion.empty() && preferences.writes == 0 && !controlBusy && !controlPending,
          "Rejected commands left side effects");
}

static void restartRejectsQueuedAndMovingMotors() {
  require(requestControl(CONTROL_DISPENSE, trayBit(1), 0), "Motor reservation rejected");
  require(!reserveRestart(), "Restart accepted while a motor command was pending");
  processControlCommand(); require(pumpOne(), "First motor timer missing");
  require(!reserveRestart(), "Restart accepted during motor movement");
  drainTimers(); assertFinished();
  require(reserveRestart(), "Restart remained blocked after motion completed");
  require(rebootPending && controlBusy && !restartScheduled, "Restart did not reserve controller");
  require(!reserveRestart(), "Duplicate restart reservation accepted");
}
static void reservedRestartBlocksMotionAndWaitsForDeadline() {
  require(reserveRestart(), "Restart reservation rejected");
  require(!requestControl(CONTROL_DISPENSE, trayBit(1), 0), "Motor accepted after restart reservation");
  scheduleRestart(1000);
  processRestart(); require(ESP.restarts == 0, "Restart happened before deadline");
  nowMs = 999; processRestart(); require(ESP.restarts == 0, "Restart happened one millisecond early");
  nowMs = 1000; processRestart(); require(ESP.restarts == 1, "Restart did not execute at deadline");
  processRestart(); require(ESP.restarts == 1, "Restart callback ran more than once");
  require(rebootPending && controlBusy && !restartScheduled, "Restart released reservation before CPU reset");
  require(!requestControl(CONTROL_ALIGN, trayBit(2), 0), "Motor accepted while reset was executing");
  require(motion.empty() && preferences.writes == 0, "Restart unexpectedly produced motor/status effects");
}
static void restartDeadlineHandlesMillisWrap() {
  nowMs = 0xfffffff0U;
  require(reserveRestart(), "Restart reservation rejected");
  scheduleRestart(32);
  require(restartAtMillis == 16, "Wrapped restart deadline mismatch");
  for (unsigned instant : {0xfffffff0U, 0xffffffffU, 0U, 15U}) {
    nowMs = instant; processRestart(); require(ESP.restarts == 0, "Wrapped deadline restarted early");
  }
  nowMs = 16; processRestart(); require(ESP.restarts == 1, "Wrapped deadline did not restart");
}
static void onlyUnscheduledRestartCanBeCancelled() {
  scheduleRestart(1000); processRestart();
  require(!rebootPending && !restartScheduled && !controlBusy && ESP.restarts == 0,
          "Unreserved restart was scheduled");
  require(reserveRestart(), "Restart reservation rejected");
  cancelReservedRestart();
  require(!rebootPending && !controlBusy && !restartScheduled, "Unscheduled reservation did not cancel");
  require(requestControl(CONTROL_DISPENSE, trayBit(1), 0), "Cancellation did not unblock motor");
  cancelReservedRestart();
  require(controlBusy && controlPending, "Restart cancellation released an unrelated motor reservation");
  processControlCommand(); drainTimers(); assertFinished();
  require(reserveRestart(), "Second restart reservation rejected");
  scheduleRestart(1000); cancelReservedRestart();
  require(rebootPending && controlBusy && restartScheduled, "Scheduled restart was cancelled");
  nowMs += 1000; processRestart(); require(ESP.restarts == 1, "Scheduled restart did not survive cancellation");
}

int main() {
  const std::vector<std::pair<const char *, std::function<void()>>> tests = {
    {"single dispense leaves due peers", singleLeavesDuePeers},
    {"group uses captured color snapshot", groupUsesOnlyColorSnapshot},
    {"group excludes resolved captured trays", groupDropsNoLongerDueTrays},
    {"fully resolved group produces no motion", emptyResolvedGroupDoesNotMove},
    {"skip persists status without motor motion", skipHasNoMotorEffects},
    {"manual-only dispense records daily status", manualOnlyStillRecordsDailyDispense},
    {"busy rejects duplicates and overlaps", busyRejectsDuplicateAndOverlappingCommands},
    {"concurrent requests reserve exactly once", simultaneousRequestsReserveExactlyOnce},
    {"alignment is one half-step without status changes", alignmentIsExactlyOneHalfStepWithoutStatusChanges},
    {"advance counts 1/3/30 cycles without status changes", advancementCountsCyclesWithoutStatusChanges},
    {"invalid tray/step/action commands never move", invalidCommandsNeverMove},
    {"restart rejects queued and moving motor commands", restartRejectsQueuedAndMovingMotors},
    {"reserved restart blocks motion and waits for deadline", reservedRestartBlocksMotionAndWaitsForDeadline},
    {"restart deadline handles millis wrap", restartDeadlineHandlesMillisWrap},
    {"only unscheduled restart can be cancelled", onlyUnscheduledRestartCanBeCancelled},
  };
  for (const auto &test : tests) {
    resetFixture();
    try { test.second(); std::cout << "PASS " << test.first << '\n'; }
    catch (const std::exception &error) {
      std::cerr << "FAIL " << test.first << ": " << error.what() << '\n'; return 1;
    }
  }
  std::cout << tests.size() << " controller tests passed (actual controller source; no hardware)\n";
}
