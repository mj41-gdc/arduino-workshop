/*

  RED+BLUE -> A0
  WHITE -> 3.3V
  BLACK -> GND

*/

#include "EasyButtons.h"
#define BP A0
EasyButtons bp;

void OnClick(int pin) {
  Serial.println("click");
}

void OnDblClick(int pin) {
  Serial.println("double click");
}
void OnLongPress(int pin) {
  Serial.println("long press");
}
void OnVLongPress(int pin) {
  Serial.println("very long press");
}
void setup()
{

  Serial.begin(115200);
  Serial.println("Button press demo");
 
  bp.Configure(BP);
  bp.OnClick = OnClick;
  bp.OnDblClick = OnDblClick;
  bp.OnLongPress = OnLongPress;
  bp.OnVLongPress = OnVLongPress;
}
void loop()
{
 bp.CheckBP();
}

