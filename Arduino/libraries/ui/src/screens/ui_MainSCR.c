#include "../ui.h"
#include <stdio.h>
void ui_MainSCR_screen_init(void)
{
    ui_MainSCR = ui_flow_screen();
    ui_HomeStatusLBL = ui_flow_label(ui_MainSCR, "Ready", 8, 5, 214);
    lv_obj_set_style_text_font(ui_HomeStatusLBL, &lv_font_montserrat_12, 0);
    ui_ClockLBL = ui_flow_label(ui_MainSCR, "", 248, 5, 64);
    lv_obj_set_style_text_align(ui_ClockLBL, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t **buttons[] = {NULL, &ui_TrayIMG1, &ui_TrayIMG2, &ui_TrayIMG3, &ui_TrayIMG4, &ui_TrayIMG5, &ui_TrayIMG6, &ui_TrayIMG7, &ui_TrayIMG8, &ui_TrayIMG9, &ui_TrayIMG10};
    lv_obj_t **numbers[] = {NULL, &ui_TrayNumLBL1, &ui_TrayNumLBL2, &ui_TrayNumLBL3, &ui_TrayNumLBL4, &ui_TrayNumLBL5, &ui_TrayNumLBL6, &ui_TrayNumLBL7, &ui_TrayNumLBL8, &ui_TrayNumLBL9, &ui_TrayNumLBL10};
    lv_obj_t **statuses[] = {NULL, &ui_TrayCheckIMG1, &ui_TrayCheckIMG2, &ui_TrayCheckIMG3, &ui_TrayCheckIMG4, &ui_TrayCheckIMG5, &ui_TrayCheckIMG6, &ui_TrayCheckIMG7, &ui_TrayCheckIMG8, &ui_TrayCheckIMG9, &ui_TrayCheckIMG10};
    lv_obj_t **names[] = {NULL, &ui_TrayLBL1, &ui_TrayLBL2, &ui_TrayLBL3, &ui_TrayLBL4, &ui_TrayLBL5, &ui_TrayLBL6, &ui_TrayLBL7, &ui_TrayLBL8, &ui_TrayLBL9, &ui_TrayLBL10};
    for (int i = 1; i <= 10; ++i) {
        char number[8];
        snprintf(number, sizeof(number), "%d", i);
        int row = (i - 1) / 3, col = (i - 1) % 3;
        *buttons[i] = ui_flow_button(ui_MainSCR, number, 8 + col * 103, 26 + row * 52, 98, 47, numbers[i]);
        lv_obj_set_style_text_font(*numbers[i], &lv_font_montserrat_16, 0);
        *statuses[i] = lv_label_create(*buttons[i]);
        lv_label_set_text(*statuses[i], LV_SYMBOL_OK);
        lv_obj_align(*statuses[i], LV_ALIGN_TOP_RIGHT, -1, -1);
        lv_obj_add_flag(*statuses[i], LV_OBJ_FLAG_HIDDEN);
        *names[i] = lv_label_create(*buttons[i]);
        lv_obj_add_flag(*names[i], LV_OBJ_FLAG_HIDDEN);
    }
    ui_HomeConfigBTN = ui_flow_button(ui_MainSCR, "Config", 111, 182, 201, 47, &ui_SettingBTNLBL);
    ui_SettingBTN = ui_HomeConfigBTN;

    /* Wi-Fi status is shown in the status text; retain icon hooks for startup. */
    ui_WifiIconIMG = lv_img_create(ui_MainSCR);
    ui_NoWifiIconIMG = lv_img_create(ui_MainSCR);
    lv_obj_add_flag(ui_WifiIconIMG, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_NoWifiIconIMG, LV_OBJ_FLAG_HIDDEN);

    ui_DispenseConfirmPAN = lv_obj_create(ui_MainSCR);
    lv_obj_set_pos(ui_DispenseConfirmPAN, 3, 3);
    lv_obj_set_size(ui_DispenseConfirmPAN, 314, 234);
    lv_obj_clear_flag(ui_DispenseConfirmPAN, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_DispenseConfirmPAN, 0, 0);
    lv_obj_set_style_bg_color(ui_DispenseConfirmPAN, lv_color_hex(0x192434), 0);
    lv_obj_set_style_border_color(ui_DispenseConfirmPAN, lv_color_hex(0x8AA8CC), 0);
    lv_obj_set_style_border_width(ui_DispenseConfirmPAN, 2, 0);
    ui_ConfirmHeadingLBL = ui_flow_label(ui_DispenseConfirmPAN, "Confirm dispense", 10, 9, 290);
    lv_obj_set_style_text_font(ui_ConfirmHeadingLBL, &lv_font_montserrat_16, 0);
    ui_ConfirmDetailsLBL = ui_flow_label(ui_DispenseConfirmPAN, "", 10, 35, 290);
    lv_obj_set_style_text_font(ui_ConfirmDetailsLBL, &lv_font_montserrat_12, 0);
    lv_obj_set_height(ui_ConfirmDetailsLBL, 81);
    ui_ConfirmCancelBTN = ui_flow_button(ui_DispenseConfirmPAN, "Cancel", 8, 125, 144, 46, NULL);
    ui_ConfirmSkipBTN = ui_flow_button(ui_DispenseConfirmPAN, "Skip group", 160, 125, 144, 46, &ui_ConfirmSkipLBL);
    ui_ConfirmSingleBTN = ui_flow_button(ui_DispenseConfirmPAN, "Dispense this tray", 8, 179, 144, 46, &ui_ConfirmSingleLBL);
    ui_ConfirmGroupBTN = ui_flow_button(ui_DispenseConfirmPAN, "Dispense group", 160, 179, 144, 46, &ui_ConfirmGroupLBL);
    lv_obj_set_style_bg_color(ui_ConfirmSingleBTN, lv_color_hex(0x176746), 0);
    lv_obj_set_style_bg_color(ui_ConfirmGroupBTN, lv_color_hex(0x176746), 0);
    lv_obj_set_style_bg_color(ui_ConfirmSkipBTN, lv_color_hex(0x77551E), 0);
    lv_obj_add_flag(ui_DispenseConfirmPAN, LV_OBJ_FLAG_HIDDEN);
}
