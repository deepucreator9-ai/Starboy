/*
  Starbie: a tiny motion-controlled digital pet

  This is intentionally one file. Open this .ino file in Arduino IDE, edit
  the BEGINNER SETTINGS section, and upload it to a Seeed XIAO ESP32-C3.

  Install these libraries with Sketch > Include Library > Manage Libraries:
    - Adafruit GFX Library
    - Adafruit SSD1306
    - Adafruit MPU6050
    - DHT sensor library
  Arduino IDE will offer to install their dependencies too. Click Install All.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <DHT.h>
#include <Preferences.h>
#include <math.h>

// =========================== BEGINNER SETTINGS ===========================
// Everything most builders will want to customize is in this one section.

// --- Your board's pins ----------------------------------------------------
// These are ESP32-C3 GPIO numbers, with the XIAO pin labels beside them.
const int I2C_SDA_PIN = 7;       // XIAO D4: OLED + MPU6050 SDA
const int I2C_SCL_PIN = 6;       // XIAO D5: OLED + MPU6050 SCL
const int DHT_PIN = 3;           // XIAO D1: DHT11 data
const int BUTTON_ONE_PIN = 4;    // XIAO D2: opens/confirms the radial menu
const int BUTTON_TWO_PIN = 5;    // XIAO D3: shows/hides stats

// Set this to false if your submission does not have a DHT11.
const bool USE_DHT11 = true;

// Temperature thresholds (in Celsius) for cold shivering and hot sweating
const float COLD_TEMP_C = 18.0f;
const float HOT_TEMP_C = 30.0f;

// Most parts use these addresses. Only change them if an I2C scanner says
// your part is different (common OLED alternative: 0x3D).
const uint8_t OLED_ADDRESS = 0x3C;
const uint8_t MPU6050_ADDRESS = 0x68;

// --- Starting stats -------------------------------------------------------
// Stats are 0 to 100. They only change when you select an action or shake
// Starbie; nothing slowly drains while it sits on your desk.
const int STARTING_JOY = 70;
const int STARTING_ENERGY = 75;
const int STARTING_FULLNESS = 65;

// Set true, upload once, then set it back to false if you want a fresh pet.
const bool RESET_SAVED_PET_ON_BOOT = false;

// --- Radial menu ----------------------------------------------------------
// These four actions appear at TOP, RIGHT, BOTTOM, then LEFT in the menu.
// The last word controls the matching little visual reaction.
enum PetReaction {
  NAP_REACTION,
  JUMP_REACTION,
  HEART_REACTION,
  RUN_REACTION,
};

struct MenuItem {
  const char *label;
  int joyChange;
  int energyChange;
  int fullnessChange;
  PetReaction reaction;
};

const MenuItem MENU_ITEMS[] = {
  {"NAP",   1,  18, -4, NAP_REACTION},   // top: sleep and emit Zs
  {"PLAY", 12, -9, -5, RUN_REACTION},    // right: two fast laps + hearts
  {"FEED",  3,  2,  18, JUMP_REACTION},  // bottom: munch a snack and jump
  {"PET",   7,  0,  0, HEART_REACTION},  // left: happy dance and emit hearts
};
const int MENU_ITEM_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

// --- Movement feel --------------------------------------------------------
// Bigger MENU_TILT_LIMIT makes the menu ball move less.
const float MENU_TILT_LIMIT = 6.0f;

// MPU6050 orientation: change these settings instead of rewiring your board.
// 1. If the sensor is turned 90 degrees, set SWAP_MPU_AXES to true.
// 2. If a direction feels backwards, change that direction from 1.0f to -1.0f.
const bool SWAP_MPU_AXES = false;
const float MENU_X_DIRECTION = 1.0f;
const float MENU_Y_DIRECTION = -1.0f;

// A small dead zone makes the ball rest in the center until the board is tilted.
const float MENU_CENTER_DEADZONE = 0.8f;
const float SHAKE_THRESHOLD = 7.0f;

// Shake is the only movement that has an effect outside the radial menu.
const int SHAKE_JOY_CHANGE = 5;
const int SHAKE_ENERGY_CHANGE = -2;
const int SHAKE_FULLNESS_CHANGE = -1;

// --- Your pet's bitmap and animation -------------------------------------
// This is a simplified 32 by 32, one-bit version of the supplied character.
// You can replace it with your own art later; keep these size values matched
// to the sprite array you paste in.
const int PET_SPRITE_WIDTH = 32;
const int PET_SPRITE_HEIGHT = 32;

// The pet wanders unless it is asleep. A non-nap action does a tiny shake,
// then a jump, except PLAY which runs two fast laps.
const uint16_t PET_WALK_PIXEL_MS = 70;
const uint16_t PET_PRE_JUMP_MS = 230;
const uint16_t PET_JUMP_MS = 430;
const int PET_JUMP_HEIGHT = 16;
const uint32_t NAP_DURATION_MS = 48000;  // 48 seconds: four times the old nap.
const uint16_t HEARTS_DURATION_MS = 1600;
const uint16_t PLAY_LAP_MS = 800;
const uint8_t PLAY_LAP_COUNT = 2;

// Each byte stores eight pixels, left to right. This sparse outline keeps the
// supplied creature readable on a tiny, one-bit OLED: head, eye, legs, tail,
// and a small flower-like ear accent all stay separate instead of becoming a blob.
const uint8_t PROGMEM PET_SPRITE[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x70, 0x0a, 0x00,
  0x00, 0xf8, 0x1f, 0x00,
  0x00, 0x8c, 0x3f, 0x80,
  0x00, 0x43, 0xff, 0x00,
  0x00, 0x43, 0xff, 0x00,
  0x00, 0x30, 0x7e, 0x00,
  0x00, 0x98, 0x01, 0x00,
  0x01, 0x98, 0x01, 0x80,
  0x03, 0x00, 0x10, 0xc0,
  0x03, 0x04, 0x00, 0xc0,
  0x01, 0x8b, 0x01, 0x80,
  0x01, 0xcb, 0x03, 0x80,
  0x03, 0xc0, 0x03, 0xc0,
  0x03, 0xc0, 0x03, 0xc0,
  0x01, 0xc0, 0x03, 0x80,
  0x01, 0x80, 0x01, 0x80,
  0x00, 0x60, 0x06, 0x00,
  0x00, 0x3f, 0xfc, 0x00,
  0x00, 0x7f, 0xfe, 0x00,
  0x00, 0x70, 0x0e, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};
// ===========================================================================
// You can read below without needing to understand every line. The rest of
// the sketch handles buttons, sensors, drawing, and saving automatically.

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int MENU_CENTER_X = 64;
const int MENU_CENTER_Y = 32;
const int MENU_ITEM_X[] = {64, 108, 64, 20};
const int MENU_ITEM_Y[] = {12, 32, 53, 32};
const int MENU_BALL_X_RANGE = 37;
const int MENU_BALL_Y_RANGE = 22;
const uint16_t BUTTON_DEBOUNCE_MS = 30;
const uint16_t MPU_READ_INTERVAL_MS = 30;
const uint16_t DHT_READ_INTERVAL_MS = 2200;
const uint16_t SHAKE_COOLDOWN_MS = 650;
const float STANDARD_GRAVITY = 9.80665f;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
DHT dht(DHT_PIN, DHT11);
Preferences preferences;

struct PetState {
  int joy;
  int energy;
  int fullness;
};

struct ButtonState {
  int pin;
  bool stableState;
  bool lastRawState;
  uint32_t lastChangedAt;
};

enum View {
  PET_VIEW,
  MENU_VIEW,
  STATS_VIEW,
};

enum FaceMood {
  MOOD_DEFAULT,
  MOOD_BLINK,
  MOOD_HAPPY,
  MOOD_EATING,
  MOOD_SLEEPY,
  MOOD_DIZZY,
  MOOD_HOT,
  MOOD_COLD,
  MOOD_SAD
};

PetState pet = {STARTING_JOY, STARTING_ENERGY, STARTING_FULLNESS};
ButtonState buttonOne = {BUTTON_ONE_PIN, HIGH, HIGH, 0};
ButtonState buttonTwo = {BUTTON_TWO_PIN, HIGH, HIGH, 0};
View currentView = PET_VIEW;

bool mpuFound = false;
bool dhtFound = false;
float accelerationX = 0.0f;
float accelerationY = 0.0f;
float accelerationZ = STANDARD_GRAVITY;
float temperatureC = NAN;
float humidity = NAN;
float menuBallX = MENU_CENTER_X;
float menuBallY = MENU_CENTER_Y;
float menuCenterAccelerationX = 0.0f;
float menuCenterAccelerationY = 0.0f;
int selectedMenuItem = -1;
uint32_t lastMpuReadAt = 0;
uint32_t lastDhtReadAt = 0;
uint32_t lastShakeAt = 0;
uint32_t shakeAnimationEndsAt = 0;
uint32_t dizzyUntil = 0;
uint32_t eatingUntil = 0;
uint32_t dancingUntil = 0;
uint32_t petJumpStartedAt = 0;
uint32_t nappingUntil = 0;
uint32_t heartAnimationEndsAt = 0;
uint32_t playRunStartedAt = 0;
int nappingPetX = 0;

int petWalkingX(uint32_t now);

int keepInRange(int value, int smallest, int largest) {
  return constrain(value, smallest, largest);
}

void changePet(int joyChange, int energyChange, int fullnessChange) {
  pet.joy = keepInRange(pet.joy + joyChange, 0, 100);
  pet.energy = keepInRange(pet.energy + energyChange, 0, 100);
  pet.fullness = keepInRange(pet.fullness + fullnessChange, 0, 100);
}

void loadPet() {
  preferences.begin("starbie", false);
  if (RESET_SAVED_PET_ON_BOOT) {
    preferences.clear();
  }
  pet.joy = preferences.getInt("joy", STARTING_JOY);
  pet.energy = preferences.getInt("energy", STARTING_ENERGY);
  pet.fullness = preferences.getInt("full", STARTING_FULLNESS);
}

void savePet() {
  preferences.putInt("joy", pet.joy);
  preferences.putInt("energy", pet.energy);
  preferences.putInt("full", pet.fullness);
}

void setUpButton(ButtonState &button) {
  pinMode(button.pin, INPUT_PULLUP);
  button.stableState = digitalRead(button.pin);
  button.lastRawState = button.stableState;
  button.lastChangedAt = millis();
}

// Returns true exactly once for each press, even if the switch bounces.
bool wasPressed(ButtonState &button) {
  const bool rawState = digitalRead(button.pin);
  const uint32_t now = millis();

  if (rawState != button.lastRawState) {
    button.lastRawState = rawState;
    button.lastChangedAt = now;
  }

  if (rawState != button.stableState &&
      now - button.lastChangedAt >= BUTTON_DEBOUNCE_MS) {
    button.stableState = rawState;
    return button.stableState == LOW;
  }

  return false;
}

void updateMpu() {
  if (!mpuFound || millis() - lastMpuReadAt < MPU_READ_INTERVAL_MS) {
    return;
  }
  lastMpuReadAt = millis();

  sensors_event_t acceleration;
  sensors_event_t gyro;
  sensors_event_t sensorTemperature;
  mpu.getEvent(&acceleration, &gyro, &sensorTemperature);
  accelerationX = acceleration.acceleration.x;
  accelerationY = acceleration.acceleration.y;
  accelerationZ = acceleration.acceleration.z;
}

void updateDht() {
  if (!dhtFound || millis() - lastDhtReadAt < DHT_READ_INTERVAL_MS) {
    return;
  }
  lastDhtReadAt = millis();

  const float newHumidity = dht.readHumidity();
  const float newTemperatureC = dht.readTemperature();
  if (!isnan(newHumidity)) {
    humidity = newHumidity;
  }
  if (!isnan(newTemperatureC)) {
    temperatureC = newTemperatureC;
  }
}

int petWalkingX(uint32_t now) {
  const int farthestX = SCREEN_WIDTH - PET_SPRITE_WIDTH;
  const uint32_t roundTrip = static_cast<uint32_t>(farthestX) * 2;
  const uint32_t step = (now / PET_WALK_PIXEL_MS) % roundTrip;
  return step <= farthestX ? step : roundTrip - step;
}

uint32_t playRunDuration() {
  return static_cast<uint32_t>(PLAY_LAP_MS) * PLAY_LAP_COUNT;
}

bool isNapping() {
  return nappingUntil != 0 && millis() < nappingUntil;
}

bool isPlaying() {
  return playRunStartedAt != 0 && millis() - playRunStartedAt < playRunDuration();
}

bool isHot() {
  return !isnan(temperatureC) && temperatureC >= HOT_TEMP_C;
}

bool isCold() {
  return !isnan(temperatureC) && temperatureC <= COLD_TEMP_C;
}

int playRunX(uint32_t now) {
  const int farthestX = SCREEN_WIDTH - PET_SPRITE_WIDTH;
  const uint32_t lapAge = (now - playRunStartedAt) % PLAY_LAP_MS;
  const float progress = static_cast<float>(lapAge) / PLAY_LAP_MS;
  return progress < 0.5f ? static_cast<int>(progress * 2.0f * farthestX)
                         : static_cast<int>((1.0f - progress) * 2.0f * farthestX);
}

void updatePetTimers() {
  const uint32_t now = millis();
  if (nappingUntil != 0 && !isNapping()) {
    nappingUntil = 0;  // The pet wakes naturally when its nap time is over.
  }
  if (playRunStartedAt != 0 && !isPlaying()) {
    playRunStartedAt = 0;  // PLAY is exactly two left-to-right-to-left laps.
  }
  if (eatingUntil != 0 && now >= eatingUntil) {
    eatingUntil = 0;
    petJumpStartedAt = now;  // do a little jump after finishing the snack
  }
  if (dancingUntil != 0 && now >= dancingUntil) {
    dancingUntil = 0;
  }
  if (dizzyUntil != 0 && now >= dizzyUntil) {
    dizzyUntil = 0;
  }
}

// While the menu is open, the MPU6050 acts like a tiny air mouse. The position
// at the instant Button 1 opens the menu becomes the center for that menu use.
void updateMenuBall() {
  float xTilt = accelerationX - menuCenterAccelerationX;
  float yTilt = accelerationY - menuCenterAccelerationY;

  if (SWAP_MPU_AXES) {
    const float oldXTilt = xTilt;
    xTilt = yTilt;
    yTilt = oldXTilt;
  }
  xTilt = constrain(xTilt * MENU_X_DIRECTION, -MENU_TILT_LIMIT, MENU_TILT_LIMIT);
  yTilt = constrain(yTilt * MENU_Y_DIRECTION, -MENU_TILT_LIMIT, MENU_TILT_LIMIT);
  const float targetX = MENU_CENTER_X + (xTilt / MENU_TILT_LIMIT) * MENU_BALL_X_RANGE;
  const float targetY = MENU_CENTER_Y + (yTilt / MENU_TILT_LIMIT) * MENU_BALL_Y_RANGE;

  // This small amount of smoothing makes the ball feel less jittery.
  menuBallX += (targetX - menuBallX) * 0.20f;
  menuBallY += (targetY - menuBallY) * 0.20f;

  // The menu has four full sectors, not four tiny targets. Any tilt direction
  // past the center dead zone selects its matching top/right/bottom/left action.
  if (fabsf(xTilt) < MENU_CENTER_DEADZONE && fabsf(yTilt) < MENU_CENTER_DEADZONE) {
    selectedMenuItem = -1;
  } else if (fabsf(xTilt) > fabsf(yTilt)) {
    selectedMenuItem = xTilt > 0.0f ? 1 : 3;  // right or left
  } else {
    selectedMenuItem = yTilt > 0.0f ? 2 : 0;  // bottom or top
  }
}

void checkForShake() {
  if (!mpuFound || currentView != PET_VIEW) {
    return;
  }

  const float magnitude = sqrtf(accelerationX * accelerationX +
                                accelerationY * accelerationY +
                                accelerationZ * accelerationZ);
  const uint32_t now = millis();
  if (fabsf(magnitude - STANDARD_GRAVITY) >= SHAKE_THRESHOLD &&
      now - lastShakeAt >= SHAKE_COOLDOWN_MS) {
    lastShakeAt = now;
    nappingUntil = 0;  // A shake wakes a sleeping pet right away.
    shakeAnimationEndsAt = now + 350;
    dizzyUntil = now + 1500;
    changePet(SHAKE_JOY_CHANGE, SHAKE_ENERGY_CHANGE, SHAKE_FULLNESS_CHANGE);
    savePet();
  }
}

void openMenu() {
  currentView = MENU_VIEW;
  selectedMenuItem = -1;
  menuBallX = MENU_CENTER_X;
  menuBallY = MENU_CENTER_Y;
  // Calibrate here so every menu starts from however the builder is holding it.
  menuCenterAccelerationX = accelerationX;
  menuCenterAccelerationY = accelerationY;
}

void chooseMenuItem() {
  if (selectedMenuItem == -1) {
    return;  // Keep the menu open until the ball reaches a choice.
  }

  const MenuItem &item = MENU_ITEMS[selectedMenuItem];
  changePet(item.joyChange, item.energyChange, item.fullnessChange);
  savePet();
  const uint32_t now = millis();
  nappingUntil = 0;
  heartAnimationEndsAt = 0;
  playRunStartedAt = 0;
  eatingUntil = 0;
  dancingUntil = 0;

  if (item.reaction == NAP_REACTION) {
    petJumpStartedAt = 0;
    nappingPetX = petWalkingX(now);
    nappingUntil = now + NAP_DURATION_MS;
  } else if (item.reaction == RUN_REACTION) {
    petJumpStartedAt = 0;
    playRunStartedAt = now;
    heartAnimationEndsAt = now + playRunDuration();
  } else if (item.reaction == JUMP_REACTION) {
    // FEED: eat a little berry first, then jump when done
    petJumpStartedAt = 0;
    eatingUntil = now + 1400;
  } else {
    // PET: do a little happy dance with hearts
    petJumpStartedAt = now;
    dancingUntil = now + HEARTS_DURATION_MS;
    heartAnimationEndsAt = now + HEARTS_DURATION_MS;
  }
  currentView = PET_VIEW;
}

void handleButtons() {
  if (wasPressed(buttonOne)) {
    if (currentView == MENU_VIEW) {
      chooseMenuItem();
    } else {
      openMenu();
    }
  }

  if (wasPressed(buttonTwo)) {
    // Button 2 never changes the pet. It only lets you peek at the values.
    currentView = currentView == STATS_VIEW ? PET_VIEW : STATS_VIEW;
  }
}

void drawHeart(int x, int y) {
  // A tiny seven-pixel-wide heart that stays crisp on the OLED.
  display.fillRect(x - 2, y, 2, 2, SSD1306_WHITE);
  display.fillRect(x + 1, y, 2, 2, SSD1306_WHITE);
  display.fillRect(x - 3, y + 2, 7, 2, SSD1306_WHITE);
  display.fillRect(x - 2, y + 4, 5, 1, SSD1306_WHITE);
  display.fillRect(x - 1, y + 5, 3, 1, SSD1306_WHITE);
  display.drawPixel(x, y + 6, SSD1306_WHITE);
}

void drawHearts(uint32_t now, int petX, int petY) {
  if (now >= heartAnimationEndsAt) {
    return;
  }

  const float progress =
      1.0f - static_cast<float>(heartAnimationEndsAt - now) / HEARTS_DURATION_MS;
  const int rise = static_cast<int>(progress * 18.0f);
  drawHeart(petX + 10, petY - 3 - rise);
  drawHeart(petX + 22, petY - 9 - rise / 2);
}

void drawSleepZs(uint32_t now, int petX, int petY) {
  // The Zs float upward, then loop back down while the pet is napping.
  const int rise = static_cast<int>((now / 300UL) % 15UL);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(petX + 23, petY - 2 - rise);
  display.print("z");
  display.setCursor(petX + 28, petY - 8 - rise / 2);
  display.print("z");
}

void drawSnack(uint32_t now, int petX, int petY) {
  if (now >= eatingUntil) {
    return;
  }
  const uint32_t left = eatingUntil - now;
  const int foodX = (petX < SCREEN_WIDTH - 42) ? petX + 30 : petX - 6;
  const int foodY = petY + 19;

  // shrink the snack in 3 bites
  if (left > 900) {
    display.fillCircle(foodX, foodY, 3, SSD1306_WHITE);
    display.drawPixel(foodX, foodY - 4, SSD1306_WHITE); // stem
  } else if (left > 450) {
    display.fillCircle(foodX, foodY, 2, SSD1306_WHITE);
  } else {
    display.drawPixel(foodX, foodY, SSD1306_WHITE);
  }
}

void drawDizzyStars(uint32_t now, int petX, int petY) {
  if (now >= dizzyUntil) {
    return;
  }
  float angle = now / 120.0f;
  int x1 = petX + 16 + static_cast<int>(cosf(angle) * 8.0f);
  int y1 = petY + 2 + static_cast<int>(sinf(angle) * 3.0f);
  int x2 = petX + 16 - static_cast<int>(cosf(angle) * 8.0f);
  int y2 = petY + 2 - static_cast<int>(sinf(angle) * 3.0f);

  display.drawFastHLine(x1 - 1, y1, 3, SSD1306_WHITE);
  display.drawFastVLine(x1, y1 - 1, 3, SSD1306_WHITE);
  display.drawFastHLine(x2 - 1, y2, 3, SSD1306_WHITE);
  display.drawFastVLine(x2, y2 - 1, 3, SSD1306_WHITE);
}

void drawWeatherEffects(uint32_t now, int petX, int petY) {
  if (isHot()) {
    // little sun in the top right corner
    display.fillCircle(116, 10, 4, SSD1306_WHITE);
    display.drawFastHLine(108, 10, 3, SSD1306_WHITE);
    display.drawFastHLine(122, 10, 3, SSD1306_WHITE);
    display.drawFastVLine(116, 2, 3, SSD1306_WHITE);
    display.drawFastVLine(116, 16, 3, SSD1306_WHITE);

    // sweat drop sliding down beside the head
    int dropOffset = (now / 150UL) % 8UL;
    int dropX = petX + 26;
    int dropY = petY + 10 + dropOffset;
    display.drawPixel(dropX, dropY, SSD1306_WHITE);
    display.fillRect(dropX - 1, dropY + 1, 3, 2, SSD1306_WHITE);
  } else if (isCold()) {
    // two little snowflakes drifting down near Starbie
    int fall1 = (now / 120UL) % 24UL;
    int fall2 = ((now / 120UL) + 12UL) % 24UL;
    int fx1 = constrain(petX - 6, 2, SCREEN_WIDTH - 4);
    int fx2 = constrain(petX + 34, 2, SCREEN_WIDTH - 4);

    display.drawFastHLine(fx1 - 1, petY + fall1, 3, SSD1306_WHITE);
    display.drawFastVLine(fx1, petY + fall1 - 1, 3, SSD1306_WHITE);

    display.drawFastHLine(fx2 - 1, petY + fall2, 3, SSD1306_WHITE);
    display.drawFastVLine(fx2, petY + fall2 - 1, 3, SSD1306_WHITE);
  }
}

FaceMood getPetMood(uint32_t now) {
  if (now < dizzyUntil) return MOOD_DIZZY;
  if (isNapping()) return MOOD_SLEEPY;
  if (now < eatingUntil) return MOOD_EATING;
  if (now < dancingUntil || isPlaying() || petJumpStartedAt != 0) return MOOD_HAPPY;
  if (isHot()) return MOOD_HOT;
  if (isCold()) return MOOD_COLD;
  if (pet.joy < 25 || pet.fullness < 25) return MOOD_SAD;
  if (pet.energy < 25) return MOOD_SLEEPY;
  if ((now % 3200UL) < 150UL) return MOOD_BLINK;
  return MOOD_DEFAULT;
}

void drawFace(FaceMood mood, uint32_t now, int petX, int petY) {
  if (mood == MOOD_DEFAULT) {
    return;
  }

  // clear out the inside of the head so we can draw different expressions
  int fx = petX + 10;
  int fy = petY + 14;
  display.fillRect(fx, fy, 12, 7, SSD1306_BLACK);

  if (mood == MOOD_BLINK || mood == MOOD_SLEEPY) {
    display.drawFastHLine(fx + 1, fy + 3, 3, SSD1306_WHITE);
    display.drawFastHLine(fx + 7, fy + 3, 3, SSD1306_WHITE);
  } else if (mood == MOOD_HAPPY) {
    // ^ ^ eyes
    display.drawPixel(fx + 1, fy + 3, SSD1306_WHITE);
    display.drawPixel(fx + 2, fy + 2, SSD1306_WHITE);
    display.drawPixel(fx + 3, fy + 3, SSD1306_WHITE);
    display.drawPixel(fx + 7, fy + 3, SSD1306_WHITE);
    display.drawPixel(fx + 8, fy + 2, SSD1306_WHITE);
    display.drawPixel(fx + 9, fy + 3, SSD1306_WHITE);
    // tiny smile
    display.drawPixel(fx + 4, fy + 5, SSD1306_WHITE);
    display.drawPixel(fx + 5, fy + 6, SSD1306_WHITE);
    display.drawPixel(fx + 6, fy + 5, SSD1306_WHITE);
  } else if (mood == MOOD_EATING) {
    display.drawFastHLine(fx + 1, fy + 2, 3, SSD1306_WHITE);
    display.drawFastHLine(fx + 7, fy + 2, 3, SSD1306_WHITE);
    // chewing mouth
    if ((now / 140UL) % 2 == 0) {
      display.fillRect(fx + 4, fy + 4, 3, 3, SSD1306_WHITE);
    } else {
      display.drawFastHLine(fx + 4, fy + 5, 3, SSD1306_WHITE);
    }
  } else if (mood == MOOD_DIZZY) {
    // X X eyes
    display.drawPixel(fx + 1, fy + 1, SSD1306_WHITE);
    display.drawPixel(fx + 3, fy + 1, SSD1306_WHITE);
    display.drawPixel(fx + 2, fy + 2, SSD1306_WHITE);
    display.drawPixel(fx + 1, fy + 3, SSD1306_WHITE);
    display.drawPixel(fx + 3, fy + 3, SSD1306_WHITE);

    display.drawPixel(fx + 7, fy + 1, SSD1306_WHITE);
    display.drawPixel(fx + 9, fy + 1, SSD1306_WHITE);
    display.drawPixel(fx + 8, fy + 2, SSD1306_WHITE);
    display.drawPixel(fx + 7, fy + 3, SSD1306_WHITE);
    display.drawPixel(fx + 9, fy + 3, SSD1306_WHITE);
    display.drawFastHLine(fx + 4, fy + 5, 3, SSD1306_WHITE);
  } else if (mood == MOOD_HOT) {
    // tired flat eyes + panting tongue
    display.drawFastHLine(fx + 1, fy + 2, 3, SSD1306_WHITE);
    display.drawFastHLine(fx + 7, fy + 2, 3, SSD1306_WHITE);
    display.drawFastHLine(fx + 4, fy + 4, 3, SSD1306_WHITE);
    int pant = (now / 200UL) % 2;
    display.fillRect(fx + 5, fy + 5, 2, 1 + pant, SSD1306_WHITE);
  } else if (mood == MOOD_COLD) {
    // > < shivering eyes
    display.drawPixel(fx + 1, fy + 1, SSD1306_WHITE);
    display.drawPixel(fx + 2, fy + 2, SSD1306_WHITE);
    display.drawPixel(fx + 1, fy + 3, SSD1306_WHITE);

    display.drawPixel(fx + 9, fy + 1, SSD1306_WHITE);
    display.drawPixel(fx + 8, fy + 2, SSD1306_WHITE);
    display.drawPixel(fx + 9, fy + 3, SSD1306_WHITE);
    // wavy brrr mouth
    display.drawPixel(fx + 3, fy + 5, SSD1306_WHITE);
    display.drawPixel(fx + 4, fy + 6, SSD1306_WHITE);
    display.drawPixel(fx + 5, fy + 5, SSD1306_WHITE);
    display.drawPixel(fx + 6, fy + 6, SSD1306_WHITE);
    display.drawPixel(fx + 7, fy + 5, SSD1306_WHITE);
  } else if (mood == MOOD_SAD) {
    // T T teary eyes
    display.drawFastHLine(fx + 1, fy + 1, 3, SSD1306_WHITE);
    display.drawFastVLine(fx + 2, fy + 2, 3, SSD1306_WHITE);
    display.drawFastHLine(fx + 7, fy + 1, 3, SSD1306_WHITE);
    display.drawFastVLine(fx + 8, fy + 2, 3, SSD1306_WHITE);
  }
}

void drawPet() {
  display.clearDisplay();

  const uint32_t now = millis();
  int petX = isNapping() ? nappingPetX : petWalkingX(now);
  if (isPlaying()) {
    petX = playRunX(now);
  }
  int petY = SCREEN_HEIGHT - PET_SPRITE_HEIGHT;

  // little bounce while walking around
  if (!isNapping() && now >= eatingUntil) {
    petY -= static_cast<int>(fabsf(sinf(now / 80.0f)) * 2.0f);
  }

  // happy side-to-side shimmy when petted
  if (now < dancingUntil) {
    petX += static_cast<int>(sinf(now / 55.0f) * 3.0f);
  }

  // shiver in place if the room is cold
  if (!isNapping() && isCold()) {
    petX += ((now / 45UL) % 2 == 0) ? 1 : -1;
  }

  // The pet always wanders except during a nap; PLAY temporarily takes over.
  if (now < shakeAnimationEndsAt) {
    petX += static_cast<int>(sinf(now / 18.0f) * 3.0f);
  }

  if (!isNapping() && petJumpStartedAt != 0) {
    const uint32_t animationAge = now - petJumpStartedAt;

    if (animationAge < PET_PRE_JUMP_MS) {
      petX += static_cast<int>(sinf(now / 16.0f) * 3.0f);
    } else if (animationAge < PET_PRE_JUMP_MS + PET_JUMP_MS) {
      const float jumpProgress =
          static_cast<float>(animationAge - PET_PRE_JUMP_MS) / PET_JUMP_MS;
      petY -= static_cast<int>(sinf(jumpProgress * PI) * PET_JUMP_HEIGHT);
    } else {
      petJumpStartedAt = 0;
    }
  }

  petX = constrain(petX, 0, SCREEN_WIDTH - PET_SPRITE_WIDTH);
  display.drawBitmap(petX, petY, PET_SPRITE, PET_SPRITE_WIDTH, PET_SPRITE_HEIGHT,
                     SSD1306_WHITE);

  drawFace(getPetMood(now), now, petX, petY);

  if (isNapping()) {
    drawSleepZs(now, petX, petY);
  }
  drawSnack(now, petX, petY);
  drawDizzyStars(now, petX, petY);
  drawWeatherEffects(now, petX, petY);
  drawHearts(now, petX, petY);
}

void drawMenuItem(int item) {
  const int boxWidth = 33;
  const int boxHeight = 12;
  const int boxX = MENU_ITEM_X[item] - boxWidth / 2;
  const int boxY = MENU_ITEM_Y[item] - boxHeight / 2;
  const bool isSelected = item == selectedMenuItem;

  if (isSelected) {
    display.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
  }

  display.setTextSize(1);
  const int labelLength = strlen(MENU_ITEMS[item].label);
  display.setCursor(MENU_ITEM_X[item] - labelLength * 3, MENU_ITEM_Y[item] - 3);
  display.print(MENU_ITEMS[item].label);
  display.setTextColor(SSD1306_WHITE);
}

void drawMenu() {
  display.clearDisplay();
  // The two diagonal lines divide the screen into four big selection sectors.
  display.drawLine(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
  display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, 0, SSD1306_WHITE);
  for (int item = 0; item < MENU_ITEM_COUNT; item++) {
    drawMenuItem(item);
  }

  display.drawCircle(MENU_CENTER_X, MENU_CENTER_Y, 11, SSD1306_WHITE);
  display.fillCircle(static_cast<int>(menuBallX), static_cast<int>(menuBallY), 4,
                     SSD1306_WHITE);
}

void drawStatBar(int y, const char *label, int value) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, y);
  display.print(label);
  display.drawRect(48, y, 57, 8, SSD1306_WHITE);
  display.fillRect(49, y + 1, map(value, 0, 100, 0, 55), 6, SSD1306_WHITE);
  display.setCursor(109, y);
  display.print(value);
}

void drawStats() {
  display.clearDisplay();
  drawStatBar(5, "JOY", pet.joy);
  drawStatBar(20, "ENERGY", pet.energy);
  drawStatBar(35, "FULL", pet.fullness);

  display.setCursor(4, 53);
  if (!isnan(temperatureC)) {
    display.print("TEMP ");
    display.print(static_cast<int>(temperatureC));
    display.print("C");
  } else {
    display.print("TEMP --");
  }

  display.setCursor(76, 53);
  if (!isnan(humidity)) {
    display.print("H ");
    display.print(static_cast<int>(humidity));
    display.print("%");
  } else {
    display.print("H --");
  }
}

void drawCurrentView() {
  if (currentView == MENU_VIEW) {
    drawMenu();
  } else if (currentView == STATS_VIEW) {
    drawStats();
  } else {
    drawPet();
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  setUpButton(buttonOne);
  setUpButton(buttonTwo);
  loadPet();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found. Check power, GND, SDA, SCL, and address.");
    while (true) {
      delay(10);
    }
  }

  mpuFound = mpu.begin(MPU6050_ADDRESS, &Wire);
  if (mpuFound) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    Serial.println("MPU6050 not found. The menu will not move until it is connected.");
  }

  if (USE_DHT11) {
    dht.begin();
    dhtFound = true;
  }

  drawCurrentView();
}

void loop() {
  updateMpu();
  updateDht();
  updatePetTimers();
  handleButtons();

  if (currentView == MENU_VIEW) {
    updateMenuBall();
  } else {
    checkForShake();
  }

  drawCurrentView();
  delay(16);  // Smooth display updates without making the code complicated.
}