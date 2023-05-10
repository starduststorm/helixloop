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
const int stepsPerRevolution = 2038;
#define MotorInterfaceType 4
AccelStepper myStepper(MotorInterfaceType, IN1, IN3, IN2, IN4);

void motorsetup() {
  myStepper.setMaxSpeed(1000.0);
	myStepper.setAcceleration(100.0);
	myStepper.setSpeed(800);
}
static int loopcount = 0;
bool flip = false;
void motorloop() {
  // if (loopcount > 500) {
  //   	myStepper.moveTo((flip ? stepsPerRevolution/2 : 0));
  //     flip = !flip;
  //     loopcount = 0;
  //     logf("flip! %i", (int)flip);
  // }
  if (myStepper.distanceToGo() == 0) {
    myStepper.moveTo((flip ? stepsPerRevolution/2 : 0));
    flip = !flip;
  }
  // logf("distanceToGo = %i, currentPosition = %i", myStepper.distanceToGo(), myStepper.currentPosition());

	myStepper.run();
	loopcount++;
}
