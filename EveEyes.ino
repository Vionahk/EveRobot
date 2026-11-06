
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ESP32Servo.h>
#include "eye_bitmap.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

// Servo
#define SERVO_PIN 13

Servo neckServo;

const int SERVO_CENTER = 1500;
const int SERVO_RANGE = 120;
const unsigned long SERVO_PERIOD = 3000;
const unsigned long SERVO_UPDATE_INTERVAL = 20;

unsigned long lastServoUpdate = 0;

// OLED displays
TwoWire wireLeft = TwoWire(0);
TwoWire wireRight = TwoWire(1);

Adafruit_SH1107 displayLeft(SCREEN_WIDTH, SCREEN_HEIGHT, &wireLeft, -1);
Adafruit_SH1107 displayRight(SCREEN_WIDTH, SCREEN_HEIGHT, &wireRight, -1);

enum EyeMode {
  EYE_NEUTRAL,
  EYE_HAPPY,
  EYE_SAD
};

struct EyeState {
  EyeMode mode = EYE_NEUTRAL;
  bool animating = false;
  unsigned long startTime = 0;
};

EyeState eyeState;

// Timing constants
const int smileRiseHeight = 20;
const int smileRiseDuration = 1000;
const int smileHoldDuration = 2000;
const int smileFallDuration = 400;
const int totalSmileTime =
    smileRiseDuration + smileHoldDuration + smileFallDuration;

const int sadCoverMaxOffset = 20;
const int sadRiseDuration = 1000;
const int sadHoldDuration = 2000;
const int sadFallDuration = 400;
const int totalSadTime =
    sadRiseDuration + sadHoldDuration + sadFallDuration;

// Blink control
int blinkStage = 0;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 4000;
const int blinkStepDelay = 60;


// Smooth servo movement
void updateServo(unsigned long now) {
  if (now - lastServoUpdate < SERVO_UPDATE_INTERVAL) {
    return;
  }

  lastServoUpdate = now;

  float phase = 2.0f * PI * (now % SERVO_PERIOD) / SERVO_PERIOD;
  int position = SERVO_CENTER + (int)(SERVO_RANGE * sinf(phase));

  neckServo.writeMicroseconds(position);
}


void drawFlippedBitmap(Adafruit_SH1107 &display, const uint8_t *bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color) {
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      uint16_t pixelX = w - 1 - i;

      if (pgm_read_byte(&bitmap[j * (w / 8) + (pixelX / 8)]) &
          (1 << (7 - (pixelX % 8)))) {
        display.drawPixel(x + i, y + j, color);
      }
    }
  }
}


void drawTiltedSmile(Adafruit_SH1107 &display, int cx, int cy,
                     int rx, int ry, int offsetY, float tilt) {
  for (int y = -ry; y <= 0; y++) {
    float fy = y / float(ry);
    float fx = sqrt(1.0 - fy * fy);
    int halfWidth = int(rx * fx);
    int shift = int(y * tilt);
    int x0 = cx - halfWidth + shift;
    int x1 = cx + halfWidth + shift;
    int drawY = cy + y - offsetY;

    display.drawFastHLine(x0, drawY, x1 - x0, SH110X_BLACK);
  }

  display.fillRect(0, cy - offsetY + 1,
                   SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_BLACK);
}


void drawTiltedFrown(Adafruit_SH1107 &display, int cx, int cy,
                     int rx, int ry, int offsetY, float tilt) {
  for (float y = 0; y <= ry; y += 0.5) {
    float fy = y / float(ry);
    float fx = sqrt(1.0 - fy * fy);
    int halfWidth = int(rx * fx);
    int shift = int(y * tilt);
    int x0 = cx - halfWidth + shift;
    int x1 = cx + halfWidth + shift;
    int drawY = cy + y + offsetY;

    display.drawFastHLine(x0, drawY, x1 - x0, SH110X_BLACK);
  }

  display.fillRect(0, 0, SCREEN_WIDTH,
                   cy + offsetY, SH110X_BLACK);
}


void drawHappyRight(Adafruit_SH1107 &display, unsigned long elapsed) {
  int smileOffset = 0;

  if (elapsed < smileRiseDuration) {
    smileOffset = map(elapsed, 0, smileRiseDuration, 0, smileRiseHeight);
  } else if (elapsed < smileRiseDuration + smileHoldDuration) {
    smileOffset = smileRiseHeight;
  } else if (elapsed < totalSmileTime) {
    int fallElapsed = elapsed - smileRiseDuration - smileHoldDuration;
    smileOffset = map(fallElapsed, 0, smileFallDuration,
                      smileRiseHeight, 0);
  }

  drawTiltedSmile(display, 84, 105, 55, 20, smileOffset, -0.5);
}


void drawHappyLeft(Adafruit_SH1107 &display, unsigned long elapsed) {
  int smileOffset = 0;

  if (elapsed < smileRiseDuration) {
    smileOffset = map(elapsed, 0, smileRiseDuration, 0, smileRiseHeight);
  } else if (elapsed < smileRiseDuration + smileHoldDuration) {
    smileOffset = smileRiseHeight;
  } else if (elapsed < totalSmileTime) {
    int fallElapsed = elapsed - smileRiseDuration - smileHoldDuration;
    smileOffset = map(fallElapsed, 0, smileFallDuration,
                      smileRiseHeight, 0);
  }

  drawTiltedSmile(display, SCREEN_WIDTH - 84, 105,
                  55, 20, smileOffset, 0.5);
}


void drawSadRight(Adafruit_SH1107 &display, unsigned long elapsed) {
  int offsetY = 0;

  if (elapsed < sadRiseDuration) {
    offsetY = map(elapsed, 0, sadRiseDuration, 0, sadCoverMaxOffset);
  } else if (elapsed < sadRiseDuration + sadHoldDuration) {
    offsetY = sadCoverMaxOffset;
  } else if (elapsed < totalSadTime) {
    int fallElapsed = elapsed - sadRiseDuration - sadHoldDuration;
    offsetY = map(fallElapsed, 0, sadFallDuration,
                  sadCoverMaxOffset, 0);
  }

  drawTiltedFrown(display, 84, 20, 75, 20, offsetY, 0.5);
}


void drawSadLeft(Adafruit_SH1107 &display, unsigned long elapsed) {
  int offsetY = 0;

  if (elapsed < sadRiseDuration) {
    offsetY = map(elapsed, 0, sadRiseDuration, 0, sadCoverMaxOffset);
  } else if (elapsed < sadRiseDuration + sadHoldDuration) {
    offsetY = sadCoverMaxOffset;
  } else if (elapsed < totalSadTime) {
    int fallElapsed = elapsed - sadRiseDuration - sadHoldDuration;
    offsetY = map(fallElapsed, 0, sadFallDuration,
                  sadCoverMaxOffset, 0);
  }

  drawTiltedFrown(display, SCREEN_WIDTH - 84, 20,
                  75, 20, offsetY, -0.5);
}


void drawBlinkLids(Adafruit_SH1107 &display, unsigned long now) {
  bool allowBlink = (eyeState.mode == EYE_NEUTRAL);

  if (allowBlink) {
    if (blinkStage == 0 && now - lastBlinkTime > blinkInterval) {
      blinkStage = 1;
      lastBlinkTime = now;
    } else if (blinkStage > 0 &&
               now - lastBlinkTime > blinkStepDelay) {
      blinkStage++;
      lastBlinkTime = now;

      if (blinkStage > 6) {
        blinkStage = 0;
        blinkInterval = random(3000, 6000);
      }
    }
  }

  if (blinkStage > 0) {
    int blinkLevel = (blinkStage <= 3) ? blinkStage : 6 - blinkStage;
    int lidHeight = (SCREEN_HEIGHT / 6) * blinkLevel;

    display.fillRect(0, 0, SCREEN_WIDTH,
                     lidHeight, SH110X_BLACK);
    display.fillRect(0, SCREEN_HEIGHT - lidHeight,
                     SCREEN_WIDTH, lidHeight, SH110X_BLACK);
  }
}


void renderEyes(unsigned long now) {
  displayRight.clearDisplay();
  displayRight.drawBitmap(0, 0, eye_bitmap,
                          EYE_WIDTH, EYE_HEIGHT, SH110X_WHITE);

  displayLeft.clearDisplay();
  drawFlippedBitmap(displayLeft, eye_bitmap, 0, 0,
                    EYE_WIDTH, EYE_HEIGHT, SH110X_WHITE);

  switch (eyeState.mode) {
    case EYE_HAPPY:
      drawHappyRight(displayRight, now - eyeState.startTime);
      drawHappyLeft(displayLeft, now - eyeState.startTime);
      break;

    case EYE_SAD:
      drawSadRight(displayRight, now - eyeState.startTime);
      drawSadLeft(displayLeft, now - eyeState.startTime);
      break;

    default:
      break;
  }

  drawBlinkLids(displayRight, now);
  drawBlinkLids(displayLeft, now);

  displayRight.display();
  displayLeft.display();
}


void updateEyeState(unsigned long now) {
  static unsigned long lastTrigger = 0;
  static bool nextSad = true;

  if (eyeState.mode == EYE_NEUTRAL &&
      now - lastTrigger > random(6000, 10000)) {
    eyeState.mode = nextSad ? EYE_SAD : EYE_HAPPY;
    eyeState.animating = true;
    eyeState.startTime = now;
    lastTrigger = now;
    nextSad = !nextSad;
  }

  if (eyeState.animating) {
    if ((eyeState.mode == EYE_HAPPY &&
         now - eyeState.startTime > totalSmileTime) ||
        (eyeState.mode == EYE_SAD &&
         now - eyeState.startTime > totalSadTime)) {
      eyeState.mode = EYE_NEUTRAL;
      eyeState.animating = false;
    }
  }
}


void setup() {
  Serial.begin(115200);

  // Initialize servo
  neckServo.setPeriodHertz(50);
  neckServo.attach(SERVO_PIN, 500, 2500);
  neckServo.writeMicroseconds(SERVO_CENTER);

  // Initialize OLEDs
  wireLeft.begin(21, 22);
  wireRight.begin(19, 18);

  if (!displayLeft.begin(0x3C, true)) {
    Serial.println("Left display error!");
  }

  if (!displayRight.begin(0x3C, true)) {
    Serial.println("Right display error!");
  }

  displayLeft.clearDisplay();
  displayRight.clearDisplay();

  displayLeft.display();
  displayRight.display();

  Serial.println("EVE eyes and servo ready!");
}


void loop() {
  unsigned long now = millis();

  updateEyeState(now);
  renderEyes(now);

  // Move servo without blocking the eye animations
  updateServo(now);

  delay(30);
}
