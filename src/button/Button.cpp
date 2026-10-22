#include "Button.h"
#include "config/Config.h"

static bool lastRawPressed = false;
static bool stablePressed = false;
static bool longPressFired = false;
static unsigned long lastRawChange = 0;
static unsigned long pressStart = 0;

static bool ledState = false;
static unsigned long ledLastChange = 0;

void buttonInit() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // кнопка замыкает пин на GND
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Самотест светодиода: три вспышки при включении
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

ButtonEvent buttonUpdate() {
  unsigned long now = millis();
  bool rawPressed = (digitalRead(BUTTON_PIN) == LOW);

  if (rawPressed != lastRawPressed) {
    lastRawPressed = rawPressed;
    lastRawChange = now;
  }

  // Состояние считается устоявшимся, если не менялось BUTTON_DEBOUNCE_MS
  if (rawPressed != stablePressed &&
      now - lastRawChange >= BUTTON_DEBOUNCE_MS) {
    stablePressed = rawPressed;
    if (stablePressed) {
      pressStart = now;
      longPressFired = false;
      return BUTTON_EVENT_PRESSED;
    }
  }

  if (stablePressed && !longPressFired && now - pressStart >= LONG_PRESS_MS) {
    longPressFired = true;
    return BUTTON_EVENT_LONG_PRESS;
  }

  return BUTTON_EVENT_NONE;
}

void buttonLedUpdate(bool screenOn) {
  unsigned long now = millis();

  // Экран включён: светодиод выключен
  if (screenOn) {
    if (ledState) {
      digitalWrite(LED_PIN, LOW);
      ledState = false;
    }
    ledLastChange = now; // после гашения экрана пауза отсчитывается заново
    return;
  }

  // Экран погашен: короткая вспышка раз в LED_BLINK_PERIOD_MS
  if (!ledState &&
      now - ledLastChange >= (LED_BLINK_PERIOD_MS - LED_BLINK_ON_MS)) {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
    ledLastChange = now;
  } else if (ledState && now - ledLastChange >= LED_BLINK_ON_MS) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
    ledLastChange = now;
  }
}