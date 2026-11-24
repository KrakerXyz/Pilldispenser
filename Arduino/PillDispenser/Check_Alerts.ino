bool reconcileDayStatus(const struct tm & timeinfo) {
  // No schedule or status reset may run against an unsynchronized boot clock.
  if (timeinfo.tm_year < 124 || rebootPending) return false;
  struct tm day = timeinfo;
  if (day.tm_hour * 60 + day.tm_min < resetHours * 60 + resetMin) --day.tm_mday;
  day.tm_hour = 12;
  day.tm_min = day.tm_sec = 0;
  day.tm_isdst = -1;
  mktime(&day);
  const int logicalDay = (day.tm_year + 1900) * 10000 + (day.tm_mon + 1) * 100 + day.tm_mday;
  static int storedDay = -1;
  if (storedDay < 0) {
    preferences.begin("appSettings", true);
    storedDay = preferences.getInt("statusDay", 0);
    preferences.end();
  }
  if (storedDay == logicalDay) return true;
  if (controlBusy && !(controlPending && activeControlKind == 0)) return false;
  // On first upgrade preserve existing status; subsequent days reconcile even
  // when the device was powered off at the configured reset time.
  if (storedDay != 0) {
    for (int i = 1; i <= 10; ++i) {
      traydisptoday[i] = false;
      traydismtoday[i] = false;
      persistTrayStatus(i);
    }
  }
  preferences.begin("appSettings", false);
  preferences.putInt("statusDay", logicalDay);
  preferences.end();
  storedDay = logicalDay;
  return true;
}

void reconcileCurrentDay() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  reconcileDayStatus(timeinfo);
}

static void check_trays_timer_cb(lv_timer_t * timer) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  if (!reconcileDayStatus(timeinfo)) return;
  bool newAlert = false;
  for (int i = 1; i <= installedTrays; ++i) {
    if (trayAlertEna[i] && !alreadyTriggered[i] && !traydisptoday[i] && !traydismtoday[i] &&
        trayHours[i] == timeinfo.tm_hour && trayMin[i] == timeinfo.tm_min) {
      traytriggered[i] = true;
      alreadyTriggered[i] = true;
      newAlert = true;
    }
  }
  if (newAlert) {
    lv_timer_create(reset_triggered_timer, 120000, NULL);
    digitalWrite(21, HIGH);
    lv_obj_t * screen = lv_scr_act();
    if (!controlBusy && (screen == ui_ClockSCR || screen == ui_BlankSCR)) lv_scr_load(ui_MainSCR);
  }
  placethings();
}

static void reset_triggered_timer(lv_timer_t * timer) {
  for (int i = 1; i <= 10; ++i) alreadyTriggered[i] = false;
  lv_timer_del(timer);
}

static void flashing_timer(lv_timer_t * timer) {
  flashing = (flashing + 1) % 4;
  refreshTrayButtons();
  uint32_t color = pixels.Color(0, 0, 0);
  bool pending = false;
  for (int i = 1; i <= installedTrays; ++i) {
    if (trayfastflash[i]) {
      color = (flashing % 2) ? index_to_pixel(trayColor[i]) : pixels.Color(0, 0, 0);
      pending = true;
      break;
    }
    if (traytriggered[i] && !pending) {
      color = flashing < 2 ? index_to_pixel(trayColor[i]) : pixels.Color(0, 0, 0);
      pending = true;
    }
  }
  showLED(color);
  if (pending && systemloaded) {
    if (inactivity_timer) lv_timer_reset(inactivity_timer);
    digitalWrite(21, HIGH);
  }
}

void placethings() {
  alertinprogress = false;
  int count = 0;
  for (int i = 1; i <= installedTrays; ++i) {
    if (traytriggered[i]) { alertinprogress = true; ++count; }
  }
  if (count) lv_label_set_text_fmt(ui_HomeStatusLBL, "%d tray%s ready", count, count == 1 ? "" : "s");
  else lv_label_set_text(ui_HomeStatusLBL, "Pill dispenser");
  refreshTrayButtons();
}

void showLED(uint32_t color) {
  pixels.setPixelColor(0, color);
  pixels.setPixelColor(1, color);
  pixels.setBrightness(50);
  pixels.show();
}
