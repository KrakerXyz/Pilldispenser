#ifndef PILL_UI_FLOW_H
#define PILL_UI_FLOW_H
#include "lvgl.h"
/* Locally maintained LVGL 8 UI. Runtime actions are bound by the sketch. */
extern lv_obj_t *ui_ConfigSCR, *ui_TraySelectSCR, *ui_TrayFixSCR, *ui_DisplaySCR;
extern lv_obj_t *ui_ConfigWifiBTN, *ui_ConfigTrayBTN, *ui_ConfigFixBTN;
extern lv_obj_t *ui_ConfigSystemBTN, *ui_ConfigDisplayBTN, *ui_ConfigBackBTN;
extern lv_obj_t *ui_TraySelectBTN[11], *ui_TraySelectLBL[11], *ui_TraySelectBackBTN;
extern lv_obj_t *ui_TrayFixBTN[11], *ui_TrayFixLBL[11], *ui_TrayFixBackBTN;
extern lv_obj_t *ui_DisplaySaveBTN, *ui_DisplayBackBTN, *ui_ScreenOffBTN;
extern lv_obj_t *ui_HomeConfigBTN, *ui_HomeStatusLBL, *ui_FixStatusLBL;
extern lv_obj_t *ui_TrayConfigTitleLBL;
extern lv_obj_t *ui_DispenseConfirmPAN, *ui_ConfirmHeadingLBL, *ui_ConfirmDetailsLBL;
extern lv_obj_t *ui_ConfirmCancelBTN, *ui_ConfirmSkipBTN, *ui_ConfirmSingleBTN, *ui_ConfirmGroupBTN;
extern lv_obj_t *ui_ConfirmSkipLBL, *ui_ConfirmSingleLBL, *ui_ConfirmGroupLBL;
lv_obj_t *ui_flow_screen(void);
lv_obj_t *ui_flow_label(lv_obj_t *parent, const char *text, int x, int y, int width);
lv_obj_t *ui_flow_button(lv_obj_t *parent, const char *text, int x, int y, int width, int height, lv_obj_t **label);
void ui_flow_confirm_layout(bool show_skip, bool show_group);
void ui_flow_init(void);
#endif
