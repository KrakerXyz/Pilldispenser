#pragma once
#include <stdint.h>

enum ControlKind { CONTROL_DISPENSE = 1, CONTROL_GROUP, CONTROL_SKIP, CONTROL_ALIGN, CONTROL_ADVANCE };
struct ControlCommand { int kind; uint16_t mask; int steps; };
volatile bool controlBusy = false;
volatile bool controlPending = false;
volatile bool rebootPending = false;
volatile bool restartScheduled = false;
volatile uint32_t restartAtMillis = 0;
extern portMUX_TYPE controlMux;
ControlCommand controlCommand = {};
int activeControlKind = 0;
uint16_t remainingDispenseMask = 0;
int confirmationTray = 0;
uint16_t confirmationGroup = 0;
bool confirmationWasDispensed = false;
bool confirmationWasDue = false;
lv_obj_t * motionOverlay = nullptr;
lv_obj_t * motionLabel = nullptr;

static inline uint16_t trayBit(int tray) {
  return tray >= 1 && tray <= 10 ? uint16_t(1U << (tray - 1)) : 0;
}

// Explicit declarations also allow the actual motor code to run in host tests.
bool reserveRestart();
void cancelReservedRestart();
void scheduleRestart(uint32_t delayMs);
void processRestart();
uint16_t dueGroupMask(int color);
bool requestControl(int kind, uint16_t mask, int steps);
bool requestFirstDueGroup(bool skip);
void processControlCommand();
void startNextDispense();
void finishControl();
void persistTrayStatus(int tray);
void setMotionOverlay(bool visible);
void openDispenseConfirmation(int tray);
void closeDispenseConfirmation();
void refreshTrayButtons();
void refreshTraySelectors();
static void dispense_step1_timer(lv_timer_t * timer);
static void dispense_step2_timer(lv_timer_t * timer);
static void dispense_step3_timer(lv_timer_t * timer);
static void dispense_step4_timer(lv_timer_t * timer);
static void dispense_step5_timer(lv_timer_t * timer);
