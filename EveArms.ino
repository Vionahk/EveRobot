#include <Servo.h>

Servo leftArm;    // pin 10
Servo rightArm;   // pin 11

const int MAX_LIFT = 90;   // 0 = arms down, 90 = arms straight out in front
const int STEP_MS  = 20;   // time between servo updates
const int MAX_JUMP = 10;   // biggest allowed change in one step (degrees)

int lift = 0;              // current lift, shared by BOTH arms

// Sync and jitter counters 
unsigned long steps = 0;
unsigned long syncErrors = 0;
int maxJump = 0;           
int reversals = 0;         
unsigned long maxTimingErrUs = 0;   
unsigned long totalSyncErrors = 0;

// One lift value drives both arm

int leftAngle(int l)  { return 90 - l; }
int rightAngle(int l) { return 90 + l; }

// Both arms the same distance from down?
bool inSync(int leftA, int rightA) {
  return (90 - leftA) == (rightA - 90);
}

void setArms(int newLift) {
  newLift = constrain(newLift, 0, MAX_LIFT);

  int jump = newLift - lift;
  if (jump < 0) jump = -jump;
  if (jump > maxJump) maxJump = jump;

  lift = newLift;
  int la = leftAngle(lift);
  int ra = rightAngle(lift);
  leftArm.write(la);
  rightArm.write(ra);

  steps++;
  if (!inSync(la, ra)) syncErrors++;
}

// Move both arms together, smoothly, to a lift over ms milliseconds
void moveArms(int target, int ms) {
  target = constrain(target, 0, MAX_LIFT);
  int start = lift;
  int stepCount = ms / STEP_MS;
  if (stepCount < 1) stepCount = 1;

  int lastDir = 0;
  unsigned long t0 = micros();

  for (int i = 1; i <= stepCount; i++) {
    float t = (float)i / stepCount;
    float e = t * t * (3 - 2 * t);   // smooth start and stop
    int next = start + (int)round((target - start) * e);

    // Jitter check 1: arm should never change direction mid-move
    int dir = (next > lift) - (next < lift);
    if (dir != 0) {
      if (lastDir != 0 && dir != lastDir) reversals++;
      lastDir = dir;
    }

    setArms(next);

    // Wait until this step's exact time, so steps stay evenly spaced
    unsigned long due = t0 + (unsigned long)ms * 1000UL * i / stepCount;
    while ((long)(micros() - due) < 0) { }

    // Jitter check 2: how late was this step?
    unsigned long late = micros() - due;
    if (late > maxTimingErrUs) maxTimingErrUs = late;
  }
}

// Movements (both arms always together)

void raiseAndLower() {
  moveArms(90, 700);
  delay(400);
  moveArms(0, 700);
}

void wave() {
  moveArms(90, 500);
  for (int i = 0; i < 3; i++) {
    moveArms(70, 250);
    moveArms(90, 250);
  }
  moveArms(0, 500);
}

void flap() {
  moveArms(45, 400);
  for (int i = 0; i < 4; i++) {
    moveArms(25, 160);
    moveArms(65, 160);
  }
  moveArms(0, 400);
}

void staircase() {
  for (int l = 22; l <= 88; l += 22) {
    moveArms(l, 300);
    delay(300);
  }
  moveArms(90, 200);
  delay(300);
  moveArms(0, 800);
}

void slowRise() {
  moveArms(90, 2000);
  delay(500);
  moveArms(0, 2000);
}

void pump() {               // quick half lifts, like cheering
  for (int i = 0; i < 4; i++) {
    moveArms(45, 200);
    moveArms(0, 200);
  }
}

void reach() {              // reach forward, small nudges, back down
  moveArms(60, 900);
  delay(300);
  for (int i = 0; i < 2; i++) {
    moveArms(70, 200);
    moveArms(60, 200);
  }
  delay(300);
  moveArms(0, 900);
}

void celebrate() {
  moveArms(90, 400);
  for (int i = 0; i < 3; i++) {
    moveArms(78, 120);
    moveArms(90, 120);
  }
  delay(300);
  moveArms(0, 600);
}

// Start position

// Send both arms to the same start position (down, both servos at 90)
// and hold there, so every run begins with the arms matched.
void goToStartPosition() {
  setArms(0);
  Serial.print("Start position: left ");
  Serial.print(leftAngle(0));
  Serial.print(", right ");
  Serial.println(rightAngle(0));
  delay(2000);
}

//Tests 

bool selfTest() {
  Serial.println("Self-test...");
  bool ok = true;

  int bad = 0;
  for (int l = 0; l <= MAX_LIFT; l++) {
    int la = leftAngle(l), ra = rightAngle(l);
    if (!inSync(la, ra)) bad++;                      // same distance from down
    if (la < 0 || la > 180 || ra < 0 || ra > 180) bad++;   // real servo angles
    if (l > 0 && !(la < leftAngle(l - 1) && ra > rightAngle(l - 1))) bad++;  // both rise
  }
  if (leftAngle(0) != 90 || rightAngle(0) != 90) bad++;   // both down at 90

  if (bad == 0) {
    Serial.println("  PASS: arms matched at every lift 0-90, both start at 90");
  } else {
    Serial.println("  FAIL: arm angles don't match");
    ok = false;
  }
  Serial.println(ok ? "Self-test PASSED" : "Self-test FAILED - arms will not move");
  return ok;
}

// Run a movement and report sync and jitter
void runMove(const char* name, void (*move)()) {
  steps = 0;
  syncErrors = 0;
  maxJump = 0;
  reversals = 0;
  maxTimingErrUs = 0;
  unsigned long t0 = millis();

  move();

  unsigned long took = millis() - t0;
  totalSyncErrors += syncErrors;
  bool ok = syncErrors == 0 && reversals == 0 && maxJump <= MAX_JUMP;

  // Printing happens only between moves, so it can't cause jitter
  Serial.print(name);
  Serial.print(": ");
  Serial.print(took);
  Serial.print(" ms, sync errors ");
  Serial.print(syncErrors);
  Serial.print(", biggest step ");
  Serial.print(maxJump);
  Serial.print(" deg, twitches ");
  Serial.print(reversals);
  Serial.print(", timing jitter ");
  Serial.print(maxTimingErrUs);
  Serial.println(ok ? " us - OK" : " us - PROBLEM");

  delay(700);
}

void setup() {
  Serial.begin(9600);
  Serial.println("EVE arms v4");

  if (!selfTest()) {
    while (true) { }
  }

  setArms(0);              // both arms down before attaching
  leftArm.attach(10);
  rightArm.attach(11);

  // Send the start position again after attaching, so both servos
  // definitely start at the same place, then hold for 2 seconds
  goToStartPosition();
}

void loop() {
  runMove("Raise and lower", raiseAndLower);
  runMove("Wave", wave);
  runMove("Flap", flap);
  runMove("Staircase", staircase);
  runMove("Slow rise", slowRise);
  runMove("Pump", pump);
  runMove("Reach", reach);
  runMove("Celebrate", celebrate);

  Serial.print("Total sync errors: ");
  Serial.println(totalSyncErrors);
  delay(2000);
}