#include "../ui.h"
/* System settings only. Display settings are created by ui_flow_init(). */
void ui_SettingSCR_screen_init(void)
{
    ui_SettingSCR = ui_flow_screen();
    ui_flow_label(ui_SettingSCR, "System", 12, 8, 296);
    ui_InstalledTrayLBL = ui_flow_label(ui_SettingSCR, "Trays installed", 12, 50, 176);
    ui_HowManyTrayDROP = lv_dropdown_create(ui_SettingSCR);
    lv_dropdown_set_options(ui_HowManyTrayDROP, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10");
    lv_obj_set_pos(ui_HowManyTrayDROP, 226, 39); lv_obj_set_size(ui_HowManyTrayDROP, 80, 36);
    ui_StartofthedayLBL = ui_flow_label(ui_SettingSCR, "Start of day", 12, 94, 148);
    ui_DayHourDROP = lv_dropdown_create(ui_SettingSCR);
    lv_dropdown_set_options(ui_DayHourDROP, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23");
    lv_obj_set_pos(ui_DayHourDROP, 170, 83); lv_obj_set_size(ui_DayHourDROP, 64, 36);
    ui_DayMinutesDROP = lv_dropdown_create(ui_SettingSCR);
    lv_dropdown_set_options(ui_DayMinutesDROP, "00\n05\n10\n15\n20\n25\n30\n35\n40\n45\n50\n55");
    lv_obj_set_pos(ui_DayMinutesDROP, 242, 83); lv_obj_set_size(ui_DayMinutesDROP, 64, 36);
    ui_DSTCHK = lv_checkbox_create(ui_SettingSCR);
    lv_checkbox_set_text(ui_DSTCHK, "Daylight saving time"); lv_obj_set_pos(ui_DSTCHK, 12, 132);
    ui_UTCLBL = ui_flow_label(ui_SettingSCR, "UTC Offset: 0", 12, 171, 140);
    ui_UTCSlider = lv_slider_create(ui_SettingSCR);
    lv_slider_set_range(ui_UTCSlider, -12, 12);
    lv_obj_set_pos(ui_UTCSlider, 164, 176); lv_obj_set_size(ui_UTCSlider, 136, 10);
    lv_obj_add_event_cb(ui_UTCSlider, ui_event_UTCSlider, LV_EVENT_VALUE_CHANGED, NULL);
    ui_SettingBackBTN1 = ui_flow_button(ui_SettingSCR, LV_SYMBOL_LEFT "  Back", 8, 198, 96, 34, &ui_SettingBackBTNLBL1);
    ui_SettingBackBTN = ui_flow_button(ui_SettingSCR, "Save & Restart", 112, 198, 200, 34, &ui_SettingBackBTNLBL);
}
