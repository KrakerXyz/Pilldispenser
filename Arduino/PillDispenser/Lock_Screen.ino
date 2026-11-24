static void passkey_event_handler(lv_event_t * e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) return;
  const bool accepted = code == LV_EVENT_READY && String(lv_textarea_get_text(ui_PassTXT)) == passcode;
  lv_textarea_set_text(ui_PassTXT, "");
  const int action = nextaction;
  nextaction = 0;
  if (accepted && action == 1) openDispenseConfirmation(selectedtray);
  else if (accepted && action == 3) lv_scr_load(ui_ConfigSCR);
  else { closeDispenseConfirmation(); lv_scr_load(ui_MainSCR); }
}
