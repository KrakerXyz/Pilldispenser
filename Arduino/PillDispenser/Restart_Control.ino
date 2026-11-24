// Reserve the same controller used by motor commands before changing settings.
// Web callbacks only set a deadline; the Arduino loop performs the restart.
bool reserveRestart() {
  portENTER_CRITICAL(&controlMux);
  if (controlBusy || rebootPending) {
    portEXIT_CRITICAL(&controlMux);
    return false;
  }
  controlBusy = true;
  rebootPending = true;
  restartScheduled = false;
  portEXIT_CRITICAL(&controlMux);
  return true;
}

// A failed Wi-Fi connection can release its reservation before scheduling a reboot.
void cancelReservedRestart() {
  portENTER_CRITICAL(&controlMux);
  if (rebootPending && !restartScheduled) {
    rebootPending = false;
    controlBusy = false;
  }
  portEXIT_CRITICAL(&controlMux);
}

void scheduleRestart(uint32_t delayMs) {
  portENTER_CRITICAL(&controlMux);
  if (rebootPending) {
    restartAtMillis = uint32_t(millis()) + delayMs;
    restartScheduled = true;
  }
  portEXIT_CRITICAL(&controlMux);
}

void processRestart() {
  bool restartNow = false;
  portENTER_CRITICAL(&controlMux);
  if (rebootPending && restartScheduled &&
      int32_t(uint32_t(millis()) - restartAtMillis) >= 0) {
    restartScheduled = false;
    restartNow = true;
  }
  portEXIT_CRITICAL(&controlMux);
  if (restartNow) ESP.restart();
}
