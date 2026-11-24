void updateTrays() {
  for (int i = 1; i <= 10; ++i) {
    trayEnabled[i] = i <= installedTrays;
    if (!trayEnabled[i]) traytriggered[i] = false;
  }
  refreshTraySelectors();
  placethings();
}

lv_color_t trayTextColor(int color) {
  return color == 2 || color == 3 ? lv_color_white() : lv_color_black();
}

void refreshTrayButtons() {
  for (int i = 1; i <= 10; ++i) {
    if (i > installedTrays) {
      lv_obj_add_flag(ui_TrayIMG[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(ui_TrayIMG[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ui_TrayIMG[i], index_to_color(trayColor[i]), 0);
    const bool dim = (traytriggered[i] && flashing < 2) || (trayfastflash[i] && flashing % 2 == 0);
    lv_obj_set_style_bg_opa(ui_TrayIMG[i], dim ? LV_OPA_50 : LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_TrayIMG[i], trayTextColor(trayColor[i]), 0);
    const bool done = traydisptoday[i] && !trayfastflash[i];
    lv_obj_set_style_border_width(ui_TrayIMG[i], done ? 4 : 1, 0);
    lv_obj_set_style_border_color(ui_TrayIMG[i], lv_color_hex(done ? 0x24E36B : 0x637184), 0);
    if (done || traydismtoday[i]) {
      lv_label_set_text(ui_TrayCheckIMG[i], done ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
      lv_obj_set_style_text_color(ui_TrayCheckIMG[i], lv_color_white(), 0);
      lv_obj_set_style_bg_color(ui_TrayCheckIMG[i], lv_color_hex(done ? 0x08632B : 0x9F2435), 0);
      lv_obj_set_style_bg_opa(ui_TrayCheckIMG[i], LV_OPA_COVER, 0);
      lv_obj_clear_flag(ui_TrayCheckIMG[i], LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(ui_TrayCheckIMG[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void checkmark() { refreshTrayButtons(); }

void refreshTraySelectors() {
  for (int i = 1; i <= 10; ++i) {
    lv_obj_t * buttons[] = {ui_TraySelectBTN[i], ui_TrayFixBTN[i]};
    for (lv_obj_t * button : buttons) {
      lv_obj_set_style_bg_color(button, index_to_color(trayColor[i]), 0);
      lv_obj_set_style_text_color(button, trayTextColor(trayColor[i]), 0);
    }
    lv_obj_set_style_text_color(ui_TraySelectLBL[i], trayTextColor(trayColor[i]), 0);
    lv_obj_set_style_text_color(ui_TrayFixLBL[i], trayTextColor(trayColor[i]), 0);
    if (i > installedTrays) lv_obj_add_state(ui_TrayFixBTN[i], LV_STATE_DISABLED);
    else lv_obj_clear_state(ui_TrayFixBTN[i], LV_STATE_DISABLED);
  }
}

static void tray_check_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  const int tray = (int)(intptr_t)lv_event_get_user_data(e);
  if (tray < 1 || tray > installedTrays) return;
  selectedtray = tray;
  if (enabledpasscode) { nextaction = 1; lv_scr_load(ui_LockSCR); }
  else openDispenseConfirmation(tray);
}

static void roller_event_handler(lv_event_t * e) {
  const uint16_t option = lv_roller_get_selected(lv_event_get_target(e));
  lv_obj_set_style_bg_color(ui_ColorPanel, index_to_color(option), LV_PART_MAIN);
}

lv_color_t index_to_color(int indexcolor) {
  lv_color_t color;
  switch (indexcolor) {
    case 0: color = lv_color_hex(0xFF0000); break; // RED
    case 1: color = lv_color_hex(0xFF00FF); break; // MAGENTA
    case 2: color = lv_color_hex(0x800080); break; // VIOLET
    case 3: color = lv_color_hex(0x0000FF); break; // BLUE
    case 4: color = lv_color_hex(0x00FFFF); break; // CYAN
    case 5: color = lv_color_hex(0x00FF00); break; // GREEN
    case 6: color = lv_color_hex(0xFFFF00); break; // YELLOW
    case 7: color = lv_color_hex(0xFFA500); break; // ORANGE
    default: color = lv_color_hex(0xFFFFFF); // Default or fallback color
  }
  return color;
}

uint32_t index_to_pixel(int indexcolor) {
  uint32_t color;
  switch (indexcolor) {
    case 0: color = pixels.Color(255, 0, 0); break; // RED
    case 1: color = pixels.Color(255, 0, 255); break; // MAGENTA
    case 2: color = pixels.Color(128, 0, 128); break; // VIOLET
    case 3: color = pixels.Color(0, 0, 255); break; // BLUE
    case 4: color = pixels.Color(0, 255, 255); break; // CYAN
    case 5: color = pixels.Color(0, 255, 0); break; // GREEN
    case 6: color = pixels.Color(255, 255, 0); break; // YELLOW
    case 7: color = pixels.Color(255, 165, 0); break; // ORANGE
    default: color = pixels.Color(0, 0, 0); // Default or fallback color, WHITE
  }
  return color;
}
