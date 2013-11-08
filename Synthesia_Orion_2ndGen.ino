#include <SPI.h>
#include <avr/sleep.h>
#include "PinChangeInt.h"
#include "pins.h"
#include "batteryStatus.h"
#include "orion.h"

boolean poweredOn = false;
boolean powerSemaphore = false;
int powerCounter = 0;

void togglePower(void) 
{
  powerSemaphore = true;
} // togglePower()


void setup() 
{
  setupPins();
  
  // Make the ADC use the nternal 2.56v reference.
  analogReference(INTERNAL);
  
  // The unit should be off on powerup.
  poweredOn = false; 
  
  // Attach button interrupts
  interrupts();
  PCintPort::attachInterrupt(PIN_BUTTON_MODE, &stepMode, RISING);
  PCintPort::attachInterrupt(PIN_BUTTON_SPEED, &stepSpeed, RISING);
  attachInterrupt(INT1, &stepBrightness, RISING);
  attachInterrupt(INT0, &togglePower, RISING);
// Does not work. ???
//  attachInterrupt(PIN_BUTTON_LEVEL, &stepBrightness, FALLING);
//  attachInterrupt(PIN_BUTTON_POWER, &togglePower, FALLING);
  
  setupBatteryStatusInterrupt();  
  setupOrion();
} // setup()


void loop() {
  
  if(powerSemaphore)
  {
  if(digitalRead(PIN_BUTTON_POWER)==HIGH)
  {
    powerCounter++; 
  } else {
    powerSemaphore = false; // Disarm the semaphore (button press time < minimum) 
    powerCounter = 0;
  }
  
  if(powerCounter>10)
    {
    poweredOn = !poweredOn;
    powerSemaphore = false;
    powerCounter = 0;
    } 
  }
  
  updateBatteryStatus(poweredOn);

  // Start up the device
  if(poweredOn && isDisabled())
    enable(true);
  
  // Shut down the device
  if(!poweredOn && isEnabled())
  {
    disable();
    // Ensure that the status LED is off
    forceStatusLightOff();
    // Define sleep type
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
   // Attach the awake interrupt to the power button
   //attachInterrupt(INT0,sleepHandler, FALLING); 
   sleep_enable();
   sleep_mode();  //sleep now
   sleep_disable(); //fully awake now 
  }

  // Update the LEDs if the device is enabled
  if(poweredOn)
  {
    updateOrion();
  }


} // loop()

// End of file.

