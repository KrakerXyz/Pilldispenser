#include "ui.h"
#include <stdio.h>

static lv_style_t screen_style, button_style;
static bool styles_ready;
static void init_styles(void)
{
    if (styles_ready) return;
    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_hex(0x121922));
    lv_style_set_text_color(&screen_style, lv_color_hex(0xF2F5FA));
    lv_style_set_pad_all(&screen_style, 0);
    lv_style_init(&button_style);
    lv_style_set_bg_color(&button_style, lv_color_hex(0x284667));
    lv_style_set_radius(&button_style, 8);
    lv_style_set_border_width(&button_style, 2);
    lv_style_set_border_color(&button_style, lv_color_hex(0x476889));
    lv_style_set_pad_all(&button_style, 3);
    lv_style_set_shadow_width(&button_style, 0);
    lv_style_set_text_color(&button_style, lv_color_hex(0xFFFFFF));
    styles_ready = true;
}

lv_obj_t *ui_ConfigSCR, *ui_TraySelectSCR, *ui_TrayFixSCR, *ui_DisplaySCR;
lv_obj_t *ui_ConfigWifiBTN, *ui_ConfigTrayBTN, *ui_ConfigFixBTN;
lv_obj_t *ui_ConfigSystemBTN, *ui_ConfigDisplayBTN, *ui_ConfigBackBTN;
lv_obj_t *ui_TraySelectBTN[11], *ui_TraySelectLBL[11], *ui_TraySelectBackBTN;
lv_obj_t *ui_TrayFixBTN[11], *ui_TrayFixLBL[11], *ui_TrayFixBackBTN;
lv_obj_t *ui_DisplaySaveBTN, *ui_DisplayBackBTN, *ui_ScreenOffBTN;
lv_obj_t *ui_HomeConfigBTN, *ui_HomeStatusLBL, *ui_FixStatusLBL;
lv_obj_t *ui_TrayConfigTitleLBL;
lv_obj_t *ui_DispenseConfirmPAN, *ui_ConfirmHeadingLBL, *ui_ConfirmDetailsLBL;
lv_obj_t *ui_ConfirmCancelBTN, *ui_ConfirmSkipBTN, *ui_ConfirmSingleBTN, *ui_ConfirmGroupBTN;
lv_obj_t *ui_ConfirmSkipLBL, *ui_ConfirmSingleLBL, *ui_ConfirmGroupLBL;

lv_obj_t *ui_flow_screen(void)
{
    init_styles();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(screen, &screen_style, 0);
    return screen;
}

lv_obj_t *ui_flow_label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    if (width > 0) lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

lv_obj_t *ui_flow_button(lv_obj_t *parent, const char *text, int x, int y, int width, int height, lv_obj_t **label)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(button, &button_style, 0);
    lv_obj_t *caption = lv_label_create(button);
    lv_obj_set_width(caption, width - 12);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_WRAP);
    lv_label_set_text(caption, text);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(caption);
    lv_obj_clear_flag(caption, LV_OBJ_FLAG_CLICKABLE);
    if (label) *label = caption;
    return button;
}

static void grid_screen(lv_obj_t **screen, const char *title, lv_obj_t **buttons, lv_obj_t **labels, lv_obj_t **back)
{
    *screen = ui_flow_screen();
    ui_flow_label(*screen, title, 8, 6, 304);
    for (int i = 1; i <= 10; ++i) {
        char number[8];
        snprintf(number, sizeof(number), "%d", i);
        int row = (i - 1) / 3, col = (i - 1) % 3;
        buttons[i] = ui_flow_button(*screen, number, 8 + col * 103, 29 + row * 51, 98, 46, &labels[i]);
        lv_obj_set_style_text_font(labels[i], &lv_font_montserrat_16, 0);
    }
    *back = ui_flow_button(*screen, LV_SYMBOL_LEFT "  Back", 111, 182, 201, 46, NULL);
}

void ui_flow_init(void)
{
    ui_ConfigSCR = ui_flow_screen();
    ui_flow_label(ui_ConfigSCR, "Configuration", 12, 8, 296);
    ui_ConfigWifiBTN = ui_flow_button(ui_ConfigSCR, "Wi-Fi", 8, 38, 148, 56, NULL);
    ui_ConfigTrayBTN = ui_flow_button(ui_ConfigSCR, "Tray Config", 164, 38, 148, 56, NULL);
    ui_ConfigFixBTN = ui_flow_button(ui_ConfigSCR, "Tray Fix", 8, 104, 148, 56, NULL);
    ui_ConfigSystemBTN = ui_flow_button(ui_ConfigSCR, "System", 164, 104, 148, 56, NULL);
    ui_ConfigDisplayBTN = ui_flow_button(ui_ConfigSCR, "Display", 8, 170, 148, 56, NULL);
    ui_ConfigBackBTN = ui_flow_button(ui_ConfigSCR, LV_SYMBOL_LEFT "  Back", 164, 170, 148, 56, NULL);

    grid_screen(&ui_TraySelectSCR, "Select a tray to configure", ui_TraySelectBTN, ui_TraySelectLBL, &ui_TraySelectBackBTN);
    grid_screen(&ui_TrayFixSCR, "Tap a tray for one half-step", ui_TrayFixBTN, ui_TrayFixLBL, &ui_TrayFixBackBTN);
    /* The first child is the grid heading; runtime can replace it while busy. */
    ui_FixStatusLBL = lv_obj_get_child(ui_TrayFixSCR, 0);

    ui_DisplaySCR = ui_flow_screen();
    ui_flow_label(ui_DisplaySCR, "Display", 12, 8, 296);
    ui_ScreensaverCHK1 = lv_checkbox_create(ui_DisplaySCR);
    lv_checkbox_set_text(ui_ScreensaverCHK1, "Clock/date screensaver");
    lv_obj_set_pos(ui_ScreensaverCHK1, 14, 53);
    ui_flow_label(ui_DisplaySCR, "After 2 minutes without touch:\nshow the clock, or turn the screen off.", 14, 91, 292);
    ui_ScreenOffBTN = ui_flow_button(ui_DisplaySCR, "Screen Off Now", 14, 143, 292, 38, NULL);
    ui_DisplayBackBTN = ui_flow_button(ui_DisplaySCR, LV_SYMBOL_LEFT "  Back", 8, 194, 148, 36, NULL);
    ui_DisplaySaveBTN = ui_flow_button(ui_DisplaySCR, "Save", 164, 194, 148, 36, NULL);
}

/* Pack the available confirmation actions without leaving empty button slots. */
void ui_flow_confirm_layout(bool show_skip, bool show_group)
{
    if (show_skip) lv_obj_clear_flag(ui_ConfirmSkipBTN, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_ConfirmSkipBTN, LV_OBJ_FLAG_HIDDEN);
    if (show_group) lv_obj_clear_flag(ui_ConfirmGroupBTN, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_ConfirmGroupBTN, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(ui_ConfirmCancelBTN, 8, show_skip ? 125 : 179);
    lv_obj_set_size(ui_ConfirmCancelBTN, 144, 46);
    lv_obj_set_pos(ui_ConfirmSkipBTN, 160, 125);
    lv_obj_set_size(ui_ConfirmSkipBTN, 144, 46);
    lv_obj_set_pos(ui_ConfirmGroupBTN, 160, 179);
    lv_obj_set_size(ui_ConfirmGroupBTN, 144, 46);
    lv_obj_set_pos(ui_ConfirmSingleBTN, show_skip ? 8 : 160, 179);
    lv_obj_set_size(ui_ConfirmSingleBTN, (show_skip && !show_group) ? 296 : 144, 46);
    lv_obj_set_width(ui_ConfirmSingleLBL, (show_skip && !show_group) ? 284 : 132);
    lv_obj_center(ui_ConfirmSingleLBL);
}
