//***************************************************************
// beat8 example.  Moves pixel position based on beat8.
//
// -Marc Miller, Jan 2016
// Updated Feb 2016 - made move speed and fade rate variables.
//***************************************************************

#define FASTLED_ESP8266_RAW_PIN_ORDER
// uncomment lines 53-54 from ~/Arduino/libraries/FastLED/src/platforms/esp/8266/fastpin_esp8266.h



#include "FastLED.h"
#define LED_TYPE WS2812B
#define DATA_PIN 10  //GPIO10 - SDD3 - needs manual hack above
#define NUM_LEDS 24
#define COLOR_ORDER GRB
#define BRIGHTNESS 50
CRGB leds[NUM_LEDS];

uint8_t gHue = 0;  // Used to cycle through rainbow.
uint8_t hueSpeed = 60;  // how fast cycle through rainbow. Lower value cycles faster
uint8_t moveSpeed = 50;  // Higher value moves pixel slower.
uint8_t fadeRate = 60;  // Use higher vadfdsfdslue to give a fading tail.
int pos;



//---------------------------------------------------------------
void setup() 
{
  Serial.begin(115200);  // Allows serial monitor output (check baud rate)
  //pinMode(1, FUNCTION_3); //use TX as pin for LED 
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}


//---------------------------------------------------------------
void loop() 
{
  EVERY_N_MILLISECONDS( hueSpeed ) { gHue++; }  // Slowly cycle through the rainbow

  beat8_tail();  // Subroutine to move the pixel!
  
  FastLED.show();  // Display the pixels.

}//end main loop



//===============================================================
void beat8_tail()
{
/*
  EVERY_N_MILLISECONDS( moveSpeed ) {
    fadeToBlackBy( leds, NUM_LEDS, fadeRate);  // Fade out pixels.
  }
  uint16_t pos = beat8(moveSpeed) % NUM_LEDS;  // modulo the position to be within NUM_LEDS
  //pos++; if(pos>=NUM_LEDS) pos=0;
  leds[pos] = CHSV( gHue, 200, 255);
  //Serial.print("pos: "); Serial.println(pos);
*/
  EVERY_N_MILLISECONDS( moveSpeed ) {
    fadeToBlackBy( leds, NUM_LEDS, fadeRate);  // Fade out pixels.
    pos++; if(pos>=NUM_LEDS) pos=0;
    leds[pos] = CHSV( gHue, 200, 255);
  }
}


//---------------------------------------------------------------
//EOF
