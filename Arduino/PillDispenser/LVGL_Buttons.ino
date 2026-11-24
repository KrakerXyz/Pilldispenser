void closeDispenseConfirmation() {
  lv_obj_add_flag(ui_DispenseConfirmPAN, LV_OBJ_FLAG_HIDDEN);
  confirmationTray = 0;
  confirmationGroup = 0;
}

String groupCaption(uint16_t mask) {
  String name;
  bool first = true;
  for (int i = 1; i <= installedTrays; ++i) {
    if (!(mask & trayBit(i))) continue;
    if (first) { name = trayNames[i]; first = false; }
    else if (trayNames[i] != name) return "group";
  }
  name.trim();
  return name.length() ? name : "group";
}

void openDispenseConfirmation(int tray) {
  if (controlBusy || tray < 1 || tray > installedTrays) return;
  confirmationTray = tray;
  confirmationWasDispensed = traydisptoday[tray];
  confirmationWasDue = traytriggered[tray];
  confirmationGroup = traytriggered[tray] ? dueGroupMask(trayColor[tray]) : 0;
  const bool multiple = confirmationGroup && (confirmationGroup & (confirmationGroup - 1));
  const String name = groupCaption(confirmationGroup);
  String heading = confirmationWasDispensed ? "Already dispensed today" : "Dispense tray " + String(tray) + "?";
  String details;
  if (confirmationWasDispensed) details = "Dispense tray " + String(tray) + " again?";
  else if (confirmationGroup) {
    details = name + " ready: ";
    bool first = true;
    for (int i = 1; i <= installedTrays; ++i) if (confirmationGroup & trayBit(i)) {
      if (!first) details += ", ";
      details += String(i);
      first = false;
    }
  } else details = trayNames[tray].length() ? trayNames[tray] : "Manual dispense";
  lv_label_set_text(ui_ConfirmHeadingLBL, heading.c_str());
  lv_label_set_text(ui_ConfirmDetailsLBL, details.c_str());
  String single = confirmationWasDispensed ? "Dispense again" : (multiple ? "Dispense only " : "Dispense ") + String(tray);
  lv_label_set_text(ui_ConfirmSingleLBL, single.c_str());
  lv_label_set_text(ui_ConfirmGroupLBL, ("Dispense " + name).c_str());
  lv_label_set_text(ui_ConfirmSkipLBL, ("Skip " + name).c_str());
  if (multiple) lv_obj_clear_flag(ui_ConfirmGroupBTN, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(ui_ConfirmGroupBTN, LV_OBJ_FLAG_HIDDEN);
  if (confirmationGroup) lv_obj_clear_flag(ui_ConfirmSkipBTN, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(ui_ConfirmSkipBTN, LV_OBJ_FLAG_HIDDEN);
  ui_flow_confirm_layout(confirmationGroup != 0, multiple);
  lv_obj_clear_flag(ui_DispenseConfirmPAN, LV_OBJ_FLAG_HIDDEN);
  lv_scr_load(ui_MainSCR);
}

static void confirm_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  lv_obj_t * button = lv_event_get_target(e);
  if (button == ui_ConfirmCancelBTN) { closeDispenseConfirmation(); return; }
  const int tray = confirmationTray;
  if (tray < 1 || tray > installedTrays) { closeDispenseConfirmation(); return; }
  // A remote action/reset may have changed state since the dialog opened.
  // Show the updated warning/options before accepting another action.
  if (traydisptoday[tray] != confirmationWasDispensed || traytriggered[tray] != confirmationWasDue ||
      (confirmationWasDue && dueGroupMask(trayColor[tray]) != confirmationGroup)) {
    openDispenseConfirmation(tray);
    return;
  }
  int kind = CONTROL_DISPENSE;
  uint16_t mask = trayBit(tray);
  if (button == ui_ConfirmGroupBTN) { kind = CONTROL_GROUP; mask = confirmationGroup; }
  if (button == ui_ConfirmSkipBTN) { kind = CONTROL_SKIP; mask = confirmationGroup; }
  if (requestControl(kind, mask, 0)) closeDispenseConfirmation();
}

void setMotionOverlay(bool visible) {
  if (!motionOverlay) {
    motionOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(motionOverlay, 320, 240);
    lv_obj_set_style_radius(motionOverlay, 0, 0);
    lv_obj_set_style_bg_color(motionOverlay, lv_color_hex(0x101827), 0);
    lv_obj_set_style_bg_opa(motionOverlay, LV_OPA_90, 0);
    lv_obj_clear_flag(motionOverlay, LV_OBJ_FLAG_SCROLLABLE);
    motionLabel = lv_label_create(motionOverlay);
    lv_obj_set_width(motionLabel, 280);
    lv_obj_set_style_text_align(motionLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(motionLabel, lv_color_white(), 0);
    lv_obj_center(motionLabel);
  }
  if (visible) {
    const char * action = activeControlKind == CONTROL_ALIGN ? "Aligning" : activeControlKind == CONTROL_ADVANCE ? "Adjusting" : "Dispensing";
    lv_label_set_text_fmt(motionLabel, "%s tray %d...\nPlease wait", action, traytodispense);
    lv_obj_clear_flag(motionOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(motionOverlay);
  } else lv_obj_add_flag(motionOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void home_config_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  closeDispenseConfirmation();
  if (enabledpasscode) { nextaction = 3; lv_scr_load(ui_LockSCR); }
  else lv_scr_load(ui_ConfigSCR);
}

static void config_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  lv_obj_t * button = lv_event_get_target(e);
  if (button == ui_ConfigBackBTN) lv_scr_load(ui_MainSCR);
  else if (button == ui_ConfigWifiBTN) lv_scr_load(ui_WifiSCR);
  else if (button == ui_ConfigTrayBTN) { refreshTraySelectors(); lv_scr_load(ui_TraySelectSCR); }
  else if (button == ui_ConfigFixBTN) { refreshTraySelectors(); lv_scr_load(ui_TrayFixSCR); }
  else if (button == ui_ConfigSystemBTN) { loadSettings(); lv_scr_load(ui_SettingSCR); }
  else if (button == ui_ConfigDisplayBTN) { loadSettings(); lv_scr_load(ui_DisplaySCR); }
}

static void back_config_event_handler(lv_event_t * e) {
  if (!controlBusy) lv_scr_load(ui_ConfigSCR);
}

static void tray_select_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  const int tray = (int)(intptr_t)lv_event_get_user_data(e);
  selectedtray = tray;
  loadTraySettings(tray);
  lv_label_set_text_fmt(ui_TrayConfigTitleLBL, "Tray %d", tray);
  if (tray <= installedTrays) {
    lv_obj_add_flag(ui_TraynotinstalledLBL, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(ui_DisplaceBTN, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_flag(ui_TraynotinstalledLBL, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(ui_DisplaceBTN, LV_STATE_DISABLED);
  }
  lv_scr_load(ui_TrayConfigSCR);
}

static void tray_fix_event_handler(lv_event_t * e) {
  const int tray = (int)(intptr_t)lv_event_get_user_data(e);
  requestControl(CONTROL_ALIGN, trayBit(tray), 0);
}

static void tray_config_back_event_handler(lv_event_t * e) {
  if (!controlBusy) lv_scr_load(ui_TraySelectSCR);
}

static void SettingBackBTN_event_handler(lv_event_t * e) {
  if (!reserveRestart()) return;
  saveSettings();
  lv_obj_add_state(ui_SettingBackBTN, LV_STATE_DISABLED);
  scheduleRestart(1000);
}

static void display_save_event_handler(lv_event_t * e) {
  clockSS = lv_obj_has_state(ui_ScreensaverCHK1, LV_STATE_CHECKED);
  preferences.begin("appSettings", false);
  preferences.putBool("clockSS", clockSS);
  preferences.end();
  lv_scr_load(ui_ConfigSCR);
}

static void WifiClearBTN_event_handler(lv_event_t * e) { clearWifiPreferences(); }

static void DisplaceBTN_event_handler(lv_event_t * e) {
  requestControl(CONTROL_ADVANCE, trayBit(selectedtray), lv_dropdown_get_selected(ui_DisplaceTrayDROP) + 1);
}

static void TrayCfgClearBTN_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  clearTrayPreferences(selectedtray);
  traytriggered[selectedtray] = false;
  loadTraySettings(selectedtray);
  updateTrays();
}

static void TrayCfgSaveBTN_event_handler(lv_event_t * e) {
  if (controlBusy) return;
  saveTraySettings(selectedtray);
  if (!trayAlertEna[selectedtray]) traytriggered[selectedtray] = false;
  updateTrays();
  lv_scr_load(ui_TraySelectSCR);
}

static void LCDLBL_event_handler(lv_event_t * e) {
  closeDispenseConfirmation();
  lv_scr_load(ui_BlankSCR);
  digitalWrite(21, LOW);
}

static void wifi_back_event_handler(lv_event_t * e) {
  lv_scr_load(systemloaded ? ui_ConfigSCR : ui_MainSCR);
}

void bindFlowEvents() {
  lv_obj_add_event_cb(ui_HomeConfigBTN, home_config_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_t * menu[] = {ui_ConfigWifiBTN, ui_ConfigTrayBTN, ui_ConfigFixBTN, ui_ConfigSystemBTN, ui_ConfigDisplayBTN, ui_ConfigBackBTN};
  for (lv_obj_t * button : menu) lv_obj_add_event_cb(button, config_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_t * backs[] = {ui_TraySelectBackBTN, ui_TrayFixBackBTN, ui_SettingBackBTN1, ui_DisplayBackBTN};
  for (lv_obj_t * button : backs) lv_obj_add_event_cb(button, back_config_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_t * confirms[] = {ui_ConfirmCancelBTN, ui_ConfirmSkipBTN, ui_ConfirmSingleBTN, ui_ConfirmGroupBTN};
  for (lv_obj_t * button : confirms) lv_obj_add_event_cb(button, confirm_event_handler, LV_EVENT_CLICKED, NULL);
  for (int i = 1; i <= 10; ++i) {
    lv_obj_add_event_cb(ui_TraySelectBTN[i], tray_select_event_handler, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(ui_TrayFixBTN[i], tray_fix_event_handler, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(ui_TrayIMG[i], tray_check_event_handler, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
  lv_obj_add_event_cb(ui_TrayCfgBackBTN, tray_config_back_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_DisplaySaveBTN, display_save_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_ScreenOffBTN, LCDLBL_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_SkipWifiBTN, wifi_back_event_handler, LV_EVENT_CLICKED, NULL);
}
