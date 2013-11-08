#include "WS2811.h"
#include "LPD8806.h"
#include "orion.h"
#include "gamma.h"
#include "pins.h"

byte stripBufferA[PIXEL_COUNT];
byte stripBufferB[PIXEL_COUNT];

// Semaphores for button interrupts
boolean brightnessSemaphore = false;
boolean speedSemaphore = false;
boolean modeSemaphore = false;
boolean drawSingleFrame = false;

// Debounce counters (testing showed that calling millis() is unecessary)
int brightnessCounter = 0;
int speedCounter = 0;
int modeCounter = 0;

uint32_t pixelBuffer[PIXEL_COUNT];

int animationStep; // Used for incrementing animations (0-WHEEL_RANGE)
int frameStep;     // Used to increment frame counts.
int mode;          // System mode
int syspeed;         // System animation speed control
int brightness;    // System brightness control

int frameDelayTimer = 5;
unsigned long previousMillis = 0;
unsigned long currentMillis = millis();

#if LED_TYPE == 0
    LPD8806 strip = LPD8806(PIXEL_COUNT);
#endif
#if LED_TYPE == 1
    WS2811 strip = WS2811(PIXEL_COUNT, MOSI, NEO_GRB + NEO_KHZ800);
#endif

void stepMode(void) {
  modeSemaphore = true;  
} // stepMode()

void stepSpeed(void) {
  speedSemaphore = true;
} // stepSpeed()

void stepBrightness(void) {
  brightnessSemaphore = true;
} // stepBrightness()

void enable(boolean setBegun) {
  strip.enable(setBegun);
} // enable()


void disable(void) {
  strip.disable();
}


boolean isEnabled(void) {
  return strip.isEnabled();
} // isEnabled()


boolean isDisabled(void) {
  return strip.isDisabled();
} // isDisabled()


void setupOrion() {

  for(int u = 0; u<PIXEL_COUNT; u++)
    pixelBuffer[u] = 0;
  
  // globalSpeed controls the delays in the animations. Starts low. Range is 1-5. 
  // Each animation is responsible for calibrating its own speed relative to the globalSpeed.
  syspeed = 0;
  animationStep = 0;
  frameStep = 0;
  mode = 0;

  // Range is 1 (least bright) to 255 (most bright)
  // Scaled to 0 - NUMBER_BRIGHTNESS_LEVELS
  brightness = 0;
} // setupOrion()


// All animations are controlled by a unified timing method.
// All animations must be totally non-blocking. That is, draw only one frame at a time.
void updateOrion() {
    
  if(brightnessSemaphore)
  {  
    if(digitalRead(PIN_BUTTON_LEVEL)==HIGH)
    {
      brightnessCounter++; 
    } else {
       brightnessSemaphore = false; // Disarm the semaphore (button press time < minimum) 
    }
    
    if(brightnessCounter>10)
      {
      brightnessSemaphore = false; 
      brightnessCounter = 0; 
      
      brightness++;
      
     if(brightness > NUMBER_BRIGHTNESS_LEVELS-1)
      brightness = 0;

      if(brightness == 0)
      {
        strip.setBrightness(255);    
        if(syspeed == NUMBER_SPEED_SETTINGS)
          drawSingleFrame = true;
      } else {
        strip.setBrightness((255/NUMBER_BRIGHTNESS_LEVELS)*(NUMBER_BRIGHTNESS_LEVELS-brightness));    
      }
      strip.show();
      }
  }

  if(speedSemaphore)
  {  
    if(digitalRead(PIN_BUTTON_SPEED)==HIGH)
    {
      speedCounter++; 
    } else {
      speedSemaphore = false; // Disarm the semaphore (button press time < minimum) 
    }
    
    if(speedCounter>10)
      {
      syspeed++;
      
       if(syspeed > NUMBER_SPEED_SETTINGS)
         syspeed = 0;
         
       speedSemaphore = false;
       speedCounter = 0;
      }
  }
  
  if(modeSemaphore)
  { 
    if(digitalRead(PIN_BUTTON_MODE)==HIGH)
    {
      modeCounter++; 
    } else {
      modeSemaphore = false; // Disarm the semaphore (button press time < minimum) 
    }
    
    if(modeCounter>10)
      {
      mode++;
       
      if(mode > NUMBER_OF_MODES)
        mode = 0;
        
      // Force redraw of new mode by increasing the speed from paused.
      if(syspeed == NUMBER_SPEED_SETTINGS)
         drawSingleFrame = true;
         
      frameStep = 0;
      animationStep = 0;
      modeSemaphore = false;
      modeCounter = 0;
      }
  }  
  
  // Used to store a current color for modes which cycle through colors.
  static uint32_t currentColor;

  // This is used to calibrate the speed range for different modes
  // Slow modes require a low frameDelayTimer (1-5). Fast modes require a high frameDelayTimer (5+).
  // millis() rolls over after 49 days of operation. 
  currentMillis = millis();

  // If insufficient time has elapsed since the last call just return and do nothing.
  // Each animation needs to be calibrated using frameDelayTimer to get a good range of speeds.
  if(currentMillis - previousMillis < frameDelayTimer*syspeed)    
    return;
    
  //Pause animations if speed is set to highest (slowest) setting.
  if(syspeed == NUMBER_SPEED_SETTINGS && !drawSingleFrame)
    return;
    
  if(drawSingleFrame)
    drawSingleFrame = false;
    
  previousMillis = currentMillis;

  switch(mode) {
    case 0:
      rainbow(); // Smooth rainbow animation.
      frameDelayTimer = 1;
      break;
    case 1:
      rainbowBreathing();
      frameDelayTimer = 1;
      break;
    case 2:
      plasma();
      frameDelayTimer = 10;    
      break;
    case 3:
      splitColorBuilder();
      frameDelayTimer = 5;
      break;
    case 4:
      smoothColors();
      frameDelayTimer = 5;
      break;
    case 5:
      // Single pixel random color pixel chase.
      if(frameStep == 0)
        currentColor = Wheel(random(0, WHEEL_RANGE));
      colorChase(currentColor);
      frameDelayTimer = 5;    
      break;
    case 6:
      // Random color wipe.
      if(frameStep == 0)
        currentColor = Wheel(random(0, WHEEL_RANGE));
      colorWipe(currentColor);
      frameDelayTimer = 5;    
      break;
    case 7:
      // Random color dither. This is a color to color dither (does not clear between colors).
      if(frameStep == 0)
        currentColor = Wheel(random(0, WHEEL_RANGE));
      dither(currentColor);
      frameDelayTimer = 8;    
      break;
    case 8:
      if(frameStep == 0)
        currentColor = Wheel(random(0, WHEEL_RANGE));
      scanner(currentColor);  
      frameDelayTimer = 5;    
      break;
    case 9:
      // Sin wave effect. New color every cycle.
      if(animationStep == 0)
        currentColor = Wheel(random(0, WHEEL_RANGE));
      wave(currentColor);  
      frameDelayTimer = 5;    
      break;
    case 10:
      // Random color noise animation.
      randomSparkle();
      frameDelayTimer = 3;    
      break;
    case 11:
      // Color fade-in fade-out effect
      if(frameStep==0)
        currentColor =Wheel(random(0, WHEEL_RANGE));
      
      if(animationStep<(WHEEL_RANGE/2))
        fadeIn(currentColor); 
      else
        fadeOut(currentColor);
    
      frameDelayTimer = 5;
      break;
    case 12:
      sparkler();
      frameDelayTimer = 10;
      break;
    default:
      ; // This should never happen. 
  } // switch()
  
  // Global animation frame limit of WHEEL_RANGE (for full color wheel range).
  // Large animationSteps slow down the driver.
  animationStep++;
    
  if(animationStep>WHEEL_RANGE)
    animationStep = 0;
  
  // Ensure that only as many pixels are drawn as there are in the strip.
  frameStep++;
  if(frameStep > PIXEL_COUNT)
    frameStep = 0;
} // updateOrion()


void solidColor()
{
    for (int i=0; i < strip.numPixels(); i++) {
     strip.setPixelColor(i, strip.Color(127, 127, 127));
    }
    strip.show();

}

void plasma() {
  
    double time = animationStep;
            
    for(int y = 0; y < PIXEL_COUNT; y++)
    {
        double value = sin(dist(frameStep + time, y, 64.0, 64.0) / 4.0)
                   + sin(dist(frameStep, y, 32.0, 32.0) / 4.0);
                   + sin(dist(frameStep, y + time / 7, 95.0, 32) / 3.5);
                   + sin(dist(frameStep, y, 95.0, 50.0) / 4.0);
  
      int color = int((4 + value)*WHEEL_RANGE)%WHEEL_RANGE;
      strip.setPixelColor(y, Wheel(color)); 
    }    
  strip.show();
}

void sparkler() {
  
  stripBufferA[random(0,PIXEL_COUNT)] = random(0, WHEEL_RANGE);

  for(int x = 0; x < PIXEL_COUNT; x++) 
    {
      byte newPoint = (stripBufferA[x] + stripBufferA[x+1]) / 2 - 15;
      stripBufferB[x] = newPoint;
      if(newPoint>50)
         strip.setPixelColor(x, Wheel(((newPoint/5)+animationStep)%WHEEL_RANGE));      
      if(newPoint<50)
         strip.setPixelColor(x, strip.Color(0,0,0)); 
    }
   
   strip.show();
    
    for(int x = 0; x < PIXEL_COUNT; x++) 
      stripBufferA[x] = stripBufferB[x];
  
}


void rainbowBreathing()
{
  static int shifter = 0;
  uint16_t i, j;
  int pixelCount = strip.numPixels();
  
  int ceiling = (255/NUMBER_BRIGHTNESS_LEVELS)*(NUMBER_BRIGHTNESS_LEVELS-brightness);
  
  if(animationStep<(WHEEL_RANGE/2))
  {
    uint16_t i, j;
    int pixelCount = strip.numPixels();
    // Use ceiling so that the max brightness scales with the rest of the modes.
    int fadeAnimation = map(animationStep, 0, WHEEL_RANGE/2, 0, ceiling);
    
    for (i=0; i < pixelCount; i++) 
      strip.setPixelColor(i, Wheel(((i * (WHEEL_RANGE / pixelCount))) % WHEEL_RANGE)); 
      
    strip.setBrightness(fadeAnimation);

    strip.show();   // write all the pixels out
  } else {
    uint16_t i, j;
    int pixelCount = strip.numPixels();
    // Use ceiling so that the max brightness scales with the rest of the modes.
    int scaleAS = map(animationStep, WHEEL_RANGE/2, WHEEL_RANGE, 0, ceiling);
    int fadeAnimation = ceiling-scaleAS;
          
    for (i=0; i < pixelCount; i++) 
      strip.setPixelColor(i, Wheel(((i * (WHEEL_RANGE / pixelCount))) % WHEEL_RANGE)); 
      
    strip.setBrightness(fadeAnimation);

    strip.show();   // write all the pixels out
  }
} 
  
  
void rainbow() {
  uint16_t i, j;
  int pixelCount = strip.numPixels();
  for (i=0; i < strip.numPixels(); i++) 
    strip.setPixelColor(i, Wheel(((i * WHEEL_RANGE / pixelCount) + animationStep) % WHEEL_RANGE)); 
  strip.show();   // write all the pixels out
}


void splitColorBuilder() {
  uint16_t i, j;
  int pixelCount = strip.numPixels();  
  uint32_t c = Wheel(animationStep);
  float y = (sin(PI * 1 * (float)(animationStep) / (float)strip.numPixels()/4))+1;
  byte  r, g, b, r2, g2, b2;

  // Need to decompose color into its r, g, b elements
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 
  
  r2 = 127 - (byte)((float)(127 - r) * y);
  g2 = 127 - (byte)((float)(127 - g) * y);
  b2 = 127 - (byte)((float)(127 - b) * y);
  
  pixelBuffer[0] = strip.Color(r2, g2, b2);

  for (i=0; i < (strip.numPixels()/2)+1; i++) 
  {
    strip.setPixelColor(PIXEL_COUNT/2-i, pixelBuffer[i]); 
    strip.setPixelColor(PIXEL_COUNT/2+i, pixelBuffer[i]); 
  }
  
  strip.show();   // write all the pixels out
  
  for(int k = PIXEL_COUNT-1; k>0; k--)
  {
   pixelBuffer[k] = pixelBuffer[k-1]; 
  }


}


void smoothColors() {
  uint16_t i, j;
  int pixelCount = strip.numPixels();
  uint32_t c = Wheel(((i * WHEEL_RANGE / pixelCount) + animationStep) % WHEEL_RANGE);
  pixelBuffer[0] = c;
  
      for (i=0; i < strip.numPixels(); i++) 
      {
        strip.setPixelColor(i, c);
      }
      strip.show();   // write all the pixels out
}


void fadeOut(uint32_t c)
{  

  byte  r, g, b, r2, g2, b2, r8, g8,b8;
  
  
  // Need to decompose color into its r, g, b elements
  if(LED_TYPE == 0)
  {
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 
 
  r8 = r;
  g8 = g;
  b8 = b;

  byte highColorByte = 0;
  if(g8>highColorByte)
  {highColorByte= g8; }  
  if(r8>highColorByte)
  {highColorByte= r8; }
  if(b8>highColorByte)
  {highColorByte= b8; }  

  byte lowColorByte = WHEEL_RANGE;
  if(g8 < lowColorByte)
  {lowColorByte= g8; }  
  if(r8 < lowColorByte)
  {lowColorByte= r8; }
  if(b8 < lowColorByte)
  {lowColorByte= b8; }   
  
  int y = (float)highColorByte-(float)highColorByte*(float)(2-0.005*animationStep);

      if(g8>y)
      {
        g2 =  gamma(g8-y);
      } else {
        g2 = gamma(0); 
      }
      if(r8>y)
      {
        r2 =  gamma(r8-y);
      } else {
        r2 = gamma(0); 
      }
      if(b8>y)
      {
        b2 =  gamma(b8-y);
      } else {
        b2 = gamma(0); 
      }
      
    for (int i=0; i < strip.numPixels(); i++) 
      strip.setPixelColor(i, r2, g2, b2);
    
    strip.show();   // write all the pixels out
  }
  
  if(LED_TYPE == 1)
  {
    r = (uint8_t)(c >> 16),
    g = (uint8_t)(c >>  8),
    b = (uint8_t)c;
 
  r8 = r;
  g8 = g;
  b8 = b;

  byte highColorByte = 0;
  if(g8>highColorByte)
  {highColorByte= g8; }  
  if(r8>highColorByte)
  {highColorByte= r8; }
  if(b8>highColorByte)
  {highColorByte= b8; }  

  byte lowColorByte = WHEEL_RANGE;
  if(g8 < lowColorByte)
  {lowColorByte= g8; }  
  if(r8 < lowColorByte)
  {lowColorByte= r8; }
  if(b8 < lowColorByte)
  {lowColorByte= b8; }   
  
  int y = 0 + (float)lowColorByte*((float)animationStep/(WHEEL_RANGE/2));

      if(g8>y)
      {
        g2 =  gamma(g8-y);
      } else {
        g2 = gamma(0); 
      }
      if(r8>y)
      {
        r2 =  gamma(r8-y);
      } else {
        r2 = gamma(0); 
      }
      if(b8>y)
      {
        b2 =  gamma(b8-y);
      } else {
        b2 = gamma(0); 
      }
      
    for (int i=0; i < strip.numPixels(); i++) 
      strip.setPixelColor(i, r2, g2, b2);
    
    strip.show();   // write all the pixels out
  }
}


void fadeIn(uint32_t c)
{
    byte  r, g, b, r2, g2, b2, r8, g8,b8;

  // Need to decompose color into its r, g, b elements
  if(LED_TYPE == 0)
  {
  
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 
 
  r8 = r;
  g8 = g;
  b8 = b;

  byte highColorByte = 0;
  if(g8>highColorByte)
  {highColorByte= g8; }  
  if(r8>highColorByte)
  {highColorByte= r8; }
  if(b8>highColorByte)
  {highColorByte= b8; }  

  byte lowColorByte = WHEEL_RANGE;
  if(g8 < lowColorByte)
  {lowColorByte= g8; }  
  if(r8 < lowColorByte)
  {lowColorByte= r8; }
  if(b8 < lowColorByte)
  {lowColorByte= b8; }   

  int y = (float)highColorByte - (float)highColorByte*((float)animationStep/192);

      if(g8>y)
      {
        g2 =  gamma(g8-y);
      } else {
        g2 = gamma(0); 
      }
      if(r8>y)
      {
        r2 =  gamma(r8-y);
      } else {
        r2 = gamma(0); 
      }
      if(b8>y)
      {
        b2 =  gamma(b8-y);
      } else {
        b2 = gamma(0); 
      }
    for (int i=0; i < strip.numPixels(); i++) 
      strip.setPixelColor(i, r2, g2, b2);
    
    strip.show();   // write all the pixels out
  }
  
  if(LED_TYPE == 1)
  {
    r = (uint8_t)(c >> 16),
    g = (uint8_t)(c >>  8),
    b = (uint8_t)c;

  r8 = r;
  g8 = g;
  b8 = b;

  byte highColorByte = 0;
  if(g8>highColorByte)
  {highColorByte= g8; }  
  if(r8>highColorByte)
  {highColorByte= r8; }
  if(b8>highColorByte)
  {highColorByte= b8; }  

  byte lowColorByte = 255;
  if(g8 < lowColorByte)
  {lowColorByte= g8; }  
  if(r8 < lowColorByte)
  {lowColorByte= r8; }
  if(b8 < lowColorByte)
  {lowColorByte= b8; }   

  int y = (float)lowColorByte - (float)lowColorByte*((float)animationStep/(WHEEL_RANGE/2));

      if(g8>y)
      {
        g2 =  gamma(g8-y);
      } else {
        g2 = gamma(0); 
      }
      if(r8>y)
      {
        r2 =  gamma(r8-y);
      } else {
        r2 = gamma(0); 
      }
      if(b8>y)
      {
        b2 =  gamma(b8-y);
      } else {
        b2 = gamma(0); 
      }
    for (int i=0; i < strip.numPixels(); i++) 
      strip.setPixelColor(i, r2, g2, b2);
    
    strip.show();   // write all the pixels out
  }
}


void pulseStrobe(uint32_t c)
{
    for (int i=0; i < strip.numPixels(); i++) 
    {
      if(animationStep%2)
        {
        strip.setPixelColor(i, dampenBrightness(c, animationStep));
        } else {
        strip.setPixelColor(i, dampenBrightness(c, 10)); 
        }
    
    strip.show();   // write all the pixels out  
    }
}


// Cycle through the color wheel, equally spaced around the belt
void rainbowCycle() {
  uint16_t i, j;
  for (i=0; i < strip.numPixels(); i++) 
  {
    strip.setPixelColor(i, Wheel(((i * WHEEL_RANGE / strip.numPixels()) + animationStep) % WHEEL_RANGE));
  }
  strip.show();   // write all the pixels out
  animationStep++;
}


void colorWipe(uint32_t c) 
{
  int i;
 
  for (i=0; i < frameStep; i++) 
    strip.setPixelColor(i, c);
   
    strip.show(); 
}


// Chase a dot down the strip
// Random color for each chase
void colorChase(uint32_t c) 
{
  int i;
  strip.setPixelColor(frameStep-1, 0); 
  strip.setPixelColor(frameStep, c); 
  strip.show(); // Refresh LED states
}


// An "ordered dither" fills every pixel in a sequence that looks
// sparkly and almost random, but actually follows a specific order.
void dither(uint32_t c) 
{
 
  // Determine highest bit needed to represent pixel index
  int hiBit = 0;
  int n = strip.numPixels() - 1;
  for(int bit=1; bit < 0x8000; bit <<= 1) {
    if(n & bit) hiBit = bit;
  }

  int bit, reverse;
  for(int i=frameStep; i<frameStep+1; i++) 
  {
    // Reverse the bits in i to create ordered dither:
    reverse = 0;
    for(bit=1; bit <= hiBit; bit <<= 1) {
      reverse <<= 1;
      if(i & bit) reverse |= 1;
    }
    strip.setPixelColor(reverse, c);
    strip.show();
  }
}


void randomSparkle() 
{
  long randNumber;
  
  // Determine highest bit needed to represent pixel index
  uint16_t i, j;

  randNumber = random(0, strip.numPixels()-1);
  strip.setPixelColor(randNumber, Wheel(random(0,WHEEL_RANGE))); 
  strip.show();


}


// "Larson scanner" = Cylon/KITT bouncing light effect
// Draws two sets of frames per call to double the resolution range
void scanner(uint32_t c) {

  byte  r, g, b;
  
  // Decompose color into its r, g, b elements
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 
  
  int i, j, pos, dir;

  if(frameStep == 0)
  {
    pos = 0;
    dir = 2;
  }
  
  pos = frameStep*2;
  
   if(pos >= strip.numPixels()) 
   {
     pos = 32-((frameStep-16)*2);
    }

    strip.setPixelColor(pos - 2, strip.Color(r/4, g/4, b/4)); 
    strip.setPixelColor(pos - 1, strip.Color(r/2, g/2, b/2)); 
    strip.setPixelColor(pos, strip.Color(r, g, b)); 
    strip.setPixelColor(pos + 1, strip.Color(r/2, g/2, b/2)); 
    strip.setPixelColor(pos + 2, strip.Color(r/4, g/4, b/4)); 


    strip.show();
    // If we wanted to be sneaky we could erase just the tail end
    // pixel, but it's much easier just to erase the whole thing
    // and draw a new one next time.
    for(j=-2; j<= 2; j++) 
        strip.setPixelColor(pos+j, strip.Color(0,0,0));

    strip.setPixelColor(pos - 1, strip.Color(r/4, g/4, b/4)); 
    strip.setPixelColor(pos - 0, strip.Color(r/2, g/2, b/2)); 
    strip.setPixelColor(pos+1, strip.Color(r, g, b)); 
    strip.setPixelColor(pos + 2, strip.Color(r/2, g/2, b/2)); 
    strip.setPixelColor(pos + 3, strip.Color(r/4, g/4, b/4)); 
    
    strip.show();
    // If we wanted to be sneaky we could erase just the tail end
    // pixel, but it's much easier just to erase the whole thing
    // and draw a new one next time.
    for(j=-1; j<= 3; j++) 
        strip.setPixelColor(pos+j, strip.Color(0,0,0));
}


// Sine wave effect.
// Self calibrating for pixel run length.
void wave(uint32_t c) {
  float y;
  byte  r, g, b, r2, g2, b2;

  // Need to decompose color into its r, g, b elements
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 

    for(int i=0; i<strip.numPixels(); i++) 
    {
      y = sin(PI * 1 * (float)(animationStep + i) / (float)strip.numPixels());
      if(y >= 0.0) {
        // Peaks of sine wave are white
        y  = 1.0 - y; // Translate Y to 0.0 (top) to 1.0 (center)
        r2 = 127 - (byte)((float)(127 - r) * y);
        g2 = 127 - (byte)((float)(127 - g) * y);
        b2 = 127 - (byte)((float)(127 - b) * y);
      } else {
        // Troughs of sine wave are black
        y += 1.0; // Translate Y to 0.0 (bottom) to 1.0 (center)
        r2 = (byte)((float)r * y);
        g2 = (byte)((float)g * y);
        b2 = (byte)((float)b * y);
      }
      strip.setPixelColor(i, r2, g2, b2);
    }
    strip.show();
}


//Input a value 0 to WHEEL_RANGE to get a color value.
//The colours are a transition r - g - b - back to r

uint32_t Wheel(uint16_t WheelPos)
{
  
  // LPD8806
  if(LED_TYPE == 0)
  {
    byte r, g, b;
    switch(WheelPos / 128)
    {
      case 0:
        r = 127 - WheelPos % 128; // red down
        g = WheelPos % 128;       // green up
        b = 0;                    // blue off
        break;
      case 1:
        g = 127 - WheelPos % 128; // green down
        b = WheelPos % 128;       // blue up
        r = 0;                    // red off
        break;
      case 2:
        b = 127 - WheelPos % 128; // blue down
        r = WheelPos % 128;       // red up
        g = 0;                    // green off
        break;
    }
    return(strip.Color(r,g,b));
  }
  
  // WS2811
  if(LED_TYPE == 1)
  {
    if(WheelPos < 85) {
     return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if(WheelPos < 170) {
     WheelPos -= 85;
     return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
     WheelPos -= 170;
     return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
  } 
  
  return strip.Color(10,10,10);
}

void fullWhiteTest() {

    for (int i=0; i < strip.numPixels(); i++) 
      {
     strip.setPixelColor(i, strip.Color(255,255,255));
      }
    strip.show();
}

uint32_t dampenBrightness(uint32_t c, int brightness) {

 byte  r, g, b;
  
  g = ((c >> 16) & 0x7f)/brightness;
  r = ((c >>  8) & 0x7f)/brightness;
  b = (c        & 0x7f)/brightness; 

  return(strip.Color(r,g,b));

}



