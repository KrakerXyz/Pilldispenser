#include "../ui.h"
void ui_WifiSCR_screen_init(void)
{
    ui_WifiSCR = ui_flow_screen();
    ui_ScanWifiBTN = ui_flow_button(ui_WifiSCR, "Scan", 8, 4, 80, 30, &ui_ScanWifiBTNLBL);
    ui_WifiClearBTN = ui_flow_button(ui_WifiSCR, "Forget Wi-Fi", 96, 4, 126, 30, &ui_WifiClearBTNLBL);
    ui_SkipWifiBTN = ui_flow_button(ui_WifiSCR, "Back", 230, 4, 82, 30, &ui_SkipWifiBTNLBL);
    ui_SSIDLBL = ui_flow_label(ui_WifiSCR, "SSID", 8, 50, 44);
    ui_SSIDDropDown = lv_dropdown_create(ui_WifiSCR);
    lv_dropdown_set_options(ui_SSIDDropDown, "");
    lv_obj_set_pos(ui_SSIDDropDown, 56, 40);
    lv_obj_set_size(ui_SSIDDropDown, 256, 34);
    ui_PassLBL = ui_flow_label(ui_WifiSCR, "Pass", 8, 89, 44);
    ui_PassTxtBox = lv_textarea_create(ui_WifiSCR);
    lv_obj_set_pos(ui_PassTxtBox, 56, 79);
    lv_obj_set_size(ui_PassTxtBox, 256, 34);
    lv_textarea_set_max_length(ui_PassTxtBox, 63);
    lv_textarea_set_one_line(ui_PassTxtBox, true);
    lv_textarea_set_password_mode(ui_PassTxtBox, true);
    ui_Keyboard = lv_keyboard_create(ui_WifiSCR);
    lv_obj_set_size(ui_Keyboard, 320, 120);
    lv_obj_align(ui_Keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ui_Keyboard, ui_PassTxtBox);
    ui_WifiScanPAN = lv_obj_create(ui_WifiSCR);
    lv_obj_set_pos(ui_WifiScanPAN, 4, 38);
    lv_obj_set_size(ui_WifiScanPAN, 312, 198);
    lv_obj_clear_flag(ui_WifiScanPAN, LV_OBJ_FLAG_SCROLLABLE);
    ui_WifiScanPANLBL = ui_flow_label(ui_WifiScanPAN, "Please wait...\n\nScanning Wi-Fi networks", 0, 0, 276);
    lv_obj_set_style_text_align(ui_WifiScanPANLBL, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(ui_WifiScanPANLBL);
    lv_obj_add_flag(ui_WifiScanPAN, LV_OBJ_FLAG_HIDDEN);
    ui_WIFILBL = lv_label_create(ui_WifiSCR);
    lv_obj_add_flag(ui_WIFILBL, LV_OBJ_FLAG_HIDDEN);
    ui_WifiInfoLBL = lv_label_create(ui_WifiSCR);
    lv_label_set_text(ui_WifiInfoLBL, "");
    lv_obj_add_flag(ui_WifiInfoLBL, LV_OBJ_FLAG_HIDDEN);
}
