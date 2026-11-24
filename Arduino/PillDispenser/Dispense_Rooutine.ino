// Motor commands from touch, the physical button and web run on the UI thread.
// Reserving the controller before queueing a command prevents overlapping moves.
portMUX_TYPE controlMux = portMUX_INITIALIZER_UNLOCKED;

uint16_t dueGroupMask(int color) {
  uint16_t mask = 0;
  for (int i = 1; i <= installedTrays; ++i)
    if (traytriggered[i] && trayColor[i] == color) mask |= trayBit(i);
  return mask;
}

bool requestControl(int kind, uint16_t mask, int steps) {
  if (!systemloaded || kind < CONTROL_DISPENSE || kind > CONTROL_ADVANCE) return false;
  if (installedTrays < 1 || installedTrays > 10) return false;
  const uint16_t installedMask = uint16_t((1U << installedTrays) - 1U);
  if (!mask || (mask & ~installedMask)) return false;
  if ((kind == CONTROL_DISPENSE || kind == CONTROL_ALIGN || kind == CONTROL_ADVANCE) && (mask & (mask - 1))) return false;
  if (kind == CONTROL_ADVANCE && (steps < 1 || steps > 30)) return false;
  portENTER_CRITICAL(&controlMux);
  if (controlBusy) {
    portEXIT_CRITICAL(&controlMux);
    return false;
  }
  controlBusy = true;
  controlCommand = {kind, mask, steps};
  controlPending = true;
  portEXIT_CRITICAL(&controlMux);
  return true;
}

bool requestFirstDueGroup(bool skip) {
  for (int i = 1; i <= installedTrays; ++i)
    if (traytriggered[i]) return requestControl(skip ? CONTROL_SKIP : CONTROL_GROUP, dueGroupMask(trayColor[i]), 0);
  return false;
}

void persistTrayStatus(int tray) {
  char key[20];
  preferences.begin("traySettings", false);
  snprintf(key, sizeof(key), "traydisptoday%d", tray);
  preferences.putBool(key, traydisptoday[tray]);
  snprintf(key, sizeof(key), "traydismtoday%d", tray);
  preferences.putBool(key, traydismtoday[tray]);
  preferences.end();
}

void processControlCommand() {
  if (!controlPending) return;
  // Set the logical day before recording a command accepted just after reset.
  reconcileCurrentDay();
  portENTER_CRITICAL(&controlMux);
  ControlCommand command = controlCommand;
  controlPending = false;
  portEXIT_CRITICAL(&controlMux);
  activeControlKind = command.kind;
  remainingDispenseMask = command.mask;
  if (command.kind == CONTROL_GROUP || command.kind == CONTROL_SKIP) {
    // Limit the action to still-pending trays shown in the confirmation.
    for (int i = 1; i <= 10; ++i)
      if (!traytriggered[i]) remainingDispenseMask &= ~trayBit(i);
  }
  closeDispenseConfirmation();
  if (!remainingDispenseMask) { finishControl(); return; }
  if (command.kind == CONTROL_SKIP) {
    for (int i = 1; i <= installedTrays; ++i) {
      if (!(remainingDispenseMask & trayBit(i))) continue;
      traytriggered[i] = false;
      traydismtoday[i] = true;
      traydisptoday[i] = false;
      persistTrayStatus(i);
    }
    finishControl();
    return;
  }
  displacesteps = command.steps;
  setMotionOverlay(true);
  startNextDispense();
}

void startNextDispense() {
  traytodispense = -1;
  for (int i = 1; i <= installedTrays; ++i) {
    if (remainingDispenseMask & trayBit(i)) {
      traytodispense = i;
      remainingDispenseMask &= ~trayBit(i);
      break;
    }
  }
  if (traytodispense < 1) { finishControl(); return; }
  currentpixelcolor = index_to_pixel(trayColor[traytodispense]);
  if (activeControlKind == CONTROL_DISPENSE || activeControlKind == CONTROL_GROUP) {
    traytriggered[traytodispense] = false;
    trayfastflash[traytodispense] = true;
    // Persist before motion, as before, so a reset cannot silently allow a repeat.
    // This records a dispense command, not sensor-confirmed delivery.
    traydisptoday[traytodispense] = true;
    traydismtoday[traytodispense] = false;
    persistTrayStatus(traytodispense);
  }
  setMotionOverlay(true);
  lv_timer_create(dispense_step1_timer, 250, NULL);
}

static void dispense_step1_timer(lv_timer_t * timer) {
  setServoPulse(traytodispense - 1, 180);
  lv_timer_create(dispense_step2_timer, 300, NULL);
  lv_timer_del(timer);
}

static void dispense_step2_timer(lv_timer_t * timer) {
  setServoPulse(traytodispense - 1, 70);
  if (activeControlKind == CONTROL_ALIGN)
    lv_timer_create(dispense_step5_timer, 500, NULL);
  else
    lv_timer_create(dispense_step3_timer, 500, NULL);
  lv_timer_del(timer);
}

static void dispense_step3_timer(lv_timer_t * timer) {
  setServoPulse(traytodispense - 1, 180);
  lv_timer_create(dispense_step4_timer, 300, NULL);
  lv_timer_del(timer);
}

static void dispense_step4_timer(lv_timer_t * timer) {
  setServoPulse(traytodispense - 1, 70);
  if (activeControlKind == CONTROL_ADVANCE && --displacesteps > 0)
    lv_timer_create(dispense_step1_timer, 500, NULL);
  else
    lv_timer_create(dispense_step5_timer, 500, NULL);
  lv_timer_del(timer);
}

static void dispense_step5_timer(lv_timer_t * timer) {
  detachServo(traytodispense - 1);
  trayfastflash[traytodispense] = false;
  lv_timer_del(timer);
  startNextDispense();
}

void finishControl() {
  const int completedKind = activeControlKind;
  activeControlKind = 0;
  remainingDispenseMask = 0;
  traytodispense = -1;
  solidLED = false;
  showLED(pixels.Color(0, 0, 0));
  setMotionOverlay(false);
  lv_obj_add_flag(ui_InprogressPAN, LV_OBJ_FLAG_HIDDEN);
  if (completedKind == CONTROL_ADVANCE) lv_dropdown_set_selected(ui_DisplaceTrayDROP, 0);
  portENTER_CRITICAL(&controlMux);
  controlBusy = false;
  portEXIT_CRITICAL(&controlMux);
  placethings();
  if (completedKind == CONTROL_DISPENSE || completedKind == CONTROL_GROUP || completedKind == CONTROL_SKIP)
    lv_scr_load(ui_MainSCR);
  if (inactivity_timer) lv_timer_reset(inactivity_timer);
}
