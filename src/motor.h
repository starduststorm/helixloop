#include <Arduino.h>
#include "util.h"

#define IN1 0 // PA2
#define IN2 1 // PA4
#define IN3 2 // PA10
#define IN4 3 // PA11

// #define IN1 14 // PA2
// #define IN2 17 // PA4
// #define IN3 4 // PA08
// #define IN4 3 // PA09


#include <AccelStepper.h>
static const int stepsPerRevolution = 2038;
#define MotorInterfaceType 4
static AccelStepper stepper(MotorInterfaceType, IN1, IN3, IN2, IN4);

static int phase = 0;
void motorsetup() {
  stepper.setMaxSpeed(1000.0);
	stepper.setAcceleration(100.0);
	stepper.setSpeed(800);

  // begin with some random movements to avoid synchronizing
  phase = random16(2 * stepsPerRevolution) - stepsPerRevolution;
  stepper.move(phase);
}

static bool flip = false;
void motorloop() {

  if (stepper.distanceToGo() == 0) {
    stepper.moveTo((flip ? stepsPerRevolution/2 : 0));
    flip = !flip;
  }

	stepper.run();
}
