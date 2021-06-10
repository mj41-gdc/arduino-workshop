/*

  OLED DISPLAY
  ------------
  GND -> GND
  VDD -> 3.3V
  SCK -> D1
  SDA -> D2

  SDCARD
  ------
  GND  -> GND
  MISO -> D6
  CLK  -> D5
  MOSI -> D7
  CS   -> D4
  3v3  -> 3.3V

  AUDIO
  -----
  LRC  -> D4
  BCLK -> D8
  DIN  -> RX
  GAIN -> not connected
  SD   -> not connected
  GND  -> GND
  VIN  -> 5V
  +    -> SPEAKER WHITE
  -    -> SPEAKER GREEN

  PUSH BUTTON
  -----------
  BLUE  -> A0
  WHITE -> 3.3V
  RED   -> A0
  BLACK -> GND

  LED RING
  --------
  GREEN -> SD3
  WHITE -> 5V
  BLACK -> GND
   
*/
#include <Arduino.h>
#include <ESP8266WiFiMulti.h>

#define ARDUINOJSON_ENABLE_PROGMEM 1

//LED defs
#define FASTLED_ESP8266_RAW_PIN_ORDER
// uncomment lines 53-54 from ~/Arduino/libraries/FastLED/src/platforms/esp/8266/fastpin_esp8266.h

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#include "config.h"

CRGB leds[NUM_LEDS];


//Button defs
#include "EasyButtons.h"
EasyButtons bp;


//SDCARD defs
#include <SPI.h>
#include <SD.h>

File root;

String dirs[MAXDIRDEPTH];
String filepath=ROOT_FOLDER;
String files[32];
uint8_t numfiles=0;
String MP3toPlay;


//OLED defs
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//WIFI defs
ESP8266WiFiMulti wifiMulti;
WiFiClient mqClient;

// Randomly picked stream URL
//const char *URL="http://ice.actve.net/fm-evropa2-128";
//const char *URL="http://ice7.radia.cz/cro1-64.mp3";

//Audio defs
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

AudioFileSourceHTTPStream *stream = nullptr;
AudioFileSourceSD *file = nullptr;
AudioFileSourceBuffer *buff = nullptr;
AudioGeneratorMP3 *mp3 = nullptr;
AudioOutputI2S *out = nullptr;

bool newAudioSource = false;
char audioSource[256] = "";
float onceGain = 0;

//MQTT defs
#include <PubSubClient.h>
#include <ArduinoJson.h>
#define MQTT_MAX_PACKET_SIZE 1024
PubSubClient mqttClient(mqClient);

//############################################################################
// SETUP FUNCTION
//############################################################################

void setup()
{

  Serial.begin(115200);
  randomSeed(analogRead(0));

// BUTTON 
  bp.Configure(BP);
  bp.OnClick = OnClick;
  bp.OnDblClick = OnDblClick;
  bp.OnLongPress = OnLongPress;
  bp.OnVLongPress = OnVLongPress;
//

// SD
  Serial.print("Initializing SD card...");
  if (!SD.begin(0)) { //0 means GPIO0 = D3 (changed from default GPIO15 D8)
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  root = SD.open(filepath);

  readDirectory(root, 0);
  Serial.println("done!");
//

// INIT SPIFFS
  SPIFFS.begin();
//
  
// LEDs
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
//
// LCD
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

  // Clear the buffer
  display.clearDisplay();
    display.drawBitmap(
    (display.width()  - LOGO_WIDTH ) / 2,
    (display.height() - LOGO_HEIGHT) / 2,
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
//

// WIFI
  delay(1000);
  Serial.println("Connecting to WiFi");


  WiFi.hostname(ESP_NAME);
  WiFi.mode(WIFI_STA);
  for (auto i : AP_LIST) {
    wifiMulti.addAP(i.ssid, i.passphrase);
  }
  wifiConnect();


  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");
//  

//MQTT

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(1024);
  mqttClient.setCallback(mqttCallback);
  mqttConnect(true);
//

}

//############################################################################
// WIFI FUNCTIONS
//############################################################################

bool wifiConnect() {
  Serial.print(F("Connecting to WiFi"));
  uint8_t count = 10;
  while (count-- && (wifiMulti.run() != WL_CONNECTED)) {
    Serial.print(".");
    delay(1000);
  }

  if (WiFi.isConnected()) {
    Serial.println(F("\nConnected to WiFi"));
    return true;
  } else {
    Serial.println(F("\nUnable to connect to WiFi"));
    return false;
  }
}

//############################################################################
// MQTT FUNCTIONS
//############################################################################

bool mqttConnect(bool about) {

  String cltName = String(ESP_NAME) + '_' + String(ESP.getChipId(), HEX);
  if (mqttClient.connect(cltName.c_str(), MQTT_USER, MQTT_PASSWORD)) {
    Serial.println(F("Connected to MQTT"));
    if (about) {
      mqttCmdAbout();
    } else {
      mqttClient.publish(MQTT_OUT_TOPIC, PSTR("Reconnected to MQTT"));
    }
    mqttClient.subscribe(MQTT_IN_TOPIC);
  } else {
    Serial.println(F("Unable to connect to MQTT"));
  }
  return mqttClient.connected();
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {

  StaticJsonBuffer<512> jsonInBuffer;
  //    DynamicJsonBuffer jsonInBuffer(2048);
  JsonObject &json = jsonInBuffer.parseObject(payload);

  if (!json.success()) {
    mqttClient.publish(MQTT_OUT_TOPIC, PSTR("{event:\"jsonInBuffer.parseObject(payload) failed\"}"));
    return;
  }

  json.printTo(Serial);
  Serial.println();

  // Simple commands
  if (json.containsKey("cmd")) {
    if (strcmp("break", json["cmd"]) == 0) {            // Break current action: {cmd:"break"}
      stopPlaying();
    } else if (strcmp("restart", json["cmd"]) == 0) {   // Restart ESP: {cmd:"restart"}
      ESP.restart();
      delay(500);
    } else if (strcmp("about", json["cmd"]) == 0) {     // About: {cmd:"about"}
      mqttCmdAbout();
    } else if (strcmp("list", json["cmd"]) == 0) {      // List SPIFFS files: {cmd:"list"}
      mqttCmdList();
    }
    return;
  }  

  // Set once gain: {oncegain:1.2}
  if (json.containsKey("oncegain")) {
    float g = json["oncegain"].as<float>();
    if (g > 0.01 && g < 3.0) {
      onceGain = g;
    }
  }
    
  // Set new MP3 source
  // - from stream: {"mp3":"http://www.universal-soundbank.com/sounds/7340.mp3"}
  // - from SD: {"mp3":"/mp3/song.mp3"}
  if (json.containsKey("mp3")) {
    strlcpy(audioSource, json["mp3"], sizeof(audioSource));
    newAudioSource = true;
  }
}

void mqttCmdAbout() {

  char uptimeBuffer[15];
  getUptimeDhms(uptimeBuffer, sizeof(uptimeBuffer));

  String freeSpace;
  prettyBytes(ESP.getFreeSketchSpace(), freeSpace);

  String sketchSize;
  prettyBytes(ESP.getSketchSize(), sketchSize);

  String chipSize;
  prettyBytes(ESP.getFlashChipRealSize(), chipSize);

  String freeHeap;
  prettyBytes(ESP.getFreeHeap(), freeHeap);

  Serial.println(F("Preparing about..."));

  DynamicJsonBuffer jsonBuffer;

  JsonObject &jsonRoot = jsonBuffer.createObject();
  jsonRoot[F("sdkVersion")] = ESP.getSdkVersion();
  jsonRoot[F("coreVersion")] = ESP.getFullVersion();
  jsonRoot[F("resetReason")] = ESP.getResetReason();
  jsonRoot[F("ssid")] = WiFi.SSID();
  jsonRoot[F("ip")] = WiFi.localIP().toString();
  jsonRoot[F("staMac")] = WiFi.macAddress();
  jsonRoot[F("apMac")] = WiFi.softAPmacAddress();
  jsonRoot[F("rssi")] = WiFi.RSSI();
  jsonRoot[F("chipId")] = String(ESP.getChipId(), HEX);
  jsonRoot[F("chipSize")] = chipSize;
  jsonRoot[F("sketchSize")] = sketchSize;
  jsonRoot[F("freeSpace")] = freeSpace;
  jsonRoot[F("freeHeap")] = freeHeap;
  jsonRoot[F("uptime")] = uptimeBuffer;
//  jsonRoot[F("voltage")] = (float) ESP.getVcc()/1000;

  String mqttMsg;
  jsonRoot.prettyPrintTo(mqttMsg);
  Serial.println(mqttMsg.c_str());

  mqttClient.publish(MQTT_OUT_TOPIC, mqttMsg.c_str());
}

void mqttCmdList() {

  DynamicJsonBuffer jsonBuffer;
  JsonArray &array = jsonBuffer.createArray();

  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    array.add(dir.fileName());
  }

  String mqttMsg;
  array.prettyPrintTo(mqttMsg);

  mqttClient.publish(MQTT_OUT_TOPIC, mqttMsg.c_str());
}

uint32_t getUptimeSecs() {
  static uint32_t uptime = 0;
  static uint32_t previousMillis = 0;
  uint32_t now = millis();

  uptime += (now - previousMillis) / 1000UL;
  previousMillis = now;
  return uptime;
}

void getUptimeDhms(char *output, size_t max_len) {
  uint32 d, h, m, s;
  uint32_t sec = getUptimeSecs();

  d = sec / 86400;
  sec = sec % 86400;
  h = sec / 3600;
  sec = sec % 3600;
  m = sec / 60;
  s = sec % 60;

  snprintf(output, max_len, "%dd %02d:%02d:%02d", d, h, m, s);
}

void prettyBytes(uint32_t bytes, String &output) {

  const char *suffixes[7] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
  uint8_t s = 0;
  double count = bytes;

  while (count >= 1024 && s < 7) {
    s++;
    count /= 1024;
  }
  if (count - floor(count) == 0.0) {
    output = String((int) count) + suffixes[s];

  } else {
    output = String(round(count * 10.0) / 10.0, 1) + suffixes[s];
  };
}



//############################################################################
//BUTTON FUNCTIONS
//############################################################################

void OnClick(int pin) {
  Serial.println("click");
  String randomMP3;
  randomMP3 = getRandomMP3();
  displayText(randomMP3);
  Serial.println(randomMP3);
  setMP3(randomMP3);
}

void OnDblClick(int pin) {
  Serial.println("double click");
  nextLedPattern();
}
void OnLongPress(int pin) {
  Serial.println("long press");
}
void OnVLongPress(int pin) {
  Serial.println("very long press");
}

//############################################################################
//SD CARD FUNCTIONS
//############################################################################

void readDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      filepath=filepath+dirs[i]+"/";
    }
    filepath=filepath+entry.name();
    if (entry.isDirectory()) {
      filepath = ROOT_FOLDER;
      dirs[numTabs] = entry.name();
      readDirectory(entry, numTabs + 1);
    } else {
      files[numfiles]=filepath;
      numfiles++;
      filepath = ROOT_FOLDER;
    }
    entry.close();
  }
}

//############################################################################
// LED FUNCTIONS
//############################################################################
typedef void (*EffectsList[])();
EffectsList ledEffects = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm, chasingRing };
uint8_t currentEffectNumber = 0; // Index number of which pattern is current
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextLedPattern()
{
  // add one to the current pattern number, and wrap around at the end
  currentEffectNumber  = (currentEffectNumber + 1) % ARRAY_SIZE( ledEffects );
}

void beat8_tail()
{

  EVERY_N_MILLISECONDS( moveSpeed ) {
    fadeToBlackBy( leds, NUM_LEDS, fadeRate);  // Fade out pixels.
    pos++; if(pos>=NUM_LEDS) pos=0;
    leds[pos] = CHSV( gHue, 200, 255);
  }
}


void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void chasingRing()
{

  beat8_tail();  // Subroutine to move the pixel!
  
}

//############################################################################
// DISPLAY FUNCTIONS
//############################################################################

void displayText(String text) {
  display.clearDisplay();

  display.setTextSize(1); 
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();      // Show initial text

}

//############################################################################
// AUDIO FUNCTIONS
//############################################################################

String getRandomMP3() {
  String randomFile;
  uint8_t randNumber = random(numfiles);
  randomFile=files[randNumber];
  while (!randomFile.endsWith(".mp3")) {
    randNumber = random(numfiles);
    randomFile=files[randNumber];
  }

  return(randomFile);
}

void setMP3(String name) {
  strlcpy(audioSource, name.c_str(), sizeof(audioSource));
  newAudioSource = true;
  mqttClient.publish(MQTT_OUT_TOPIC, name.c_str());
}


void playAudio() {
  newAudioSource = false;

  if (audioSource[0] == 0) {
    return;
  }

  stopPlaying();
  delay(50);

  Serial.printf_P(PSTR("\nFree heap: %d\n"), ESP.getFreeHeap());

  if (!out) {
    out = new AudioOutputI2S();
    out->SetOutputModeMono(true);
  }
  out->SetGain(onceGain ? : defaultGain);
  onceGain = 0;

  if (strncmp("http", audioSource, 4) == 0) {
    // Get MP3 from stream
    Serial.printf_P(PSTR("**MP3 stream: %s\n"), audioSource);
    stream = new AudioFileSourceHTTPStream(audioSource);
    buff = new AudioFileSourceBuffer(stream, 1024 * 2);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(buff, out);
    if (!mp3->isRunning()) {
      //Serial.println(F("Unable to play MP3"));
      stopPlaying();
    }
  } else if (strncmp("/mp3/", audioSource, 5) == 0) {
    // Get MP3 from SD
    Serial.printf_P(PSTR("**MP3 file: %s\n"), audioSource);
    file = new AudioFileSourceSD(audioSource);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(file, out);
    if (!mp3->isRunning()) {
      //Serial.println(F("Unable to play MP3"));
      stopPlaying();
    }
  }
}

bool stopPlaying() {
  bool stopped = false;

  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
    stopped = true;
  }
  if (buff) {
    buff->close();
    delete buff;
    buff = nullptr;
  }
  if (file) {
    file->close();
    delete file;
    file = nullptr;
  }
  if (stream) {
    stream->close();
    delete stream;
    stream = nullptr;
  }

  return stopped;
}

//############################################################################
// MAIN LOOP
//############################################################################

void loop()
{
unsigned long curMillis = millis();

// Handle BUTTON 
 bp.CheckBP();
//
// Handle LEDs
  ledEffects[currentEffectNumber]();
  EVERY_N_MILLISECONDS( hueSpeed ) { gHue++; }  // Slowly cycle through the rainbow

  FastLED.show();  
//
// HANDLE Wifi

  static unsigned long lastWifiMillis = 0;
  bool wifiIsConnected = WiFi.isConnected();
  if (!wifiIsConnected) {
    if (curMillis - lastWifiMillis > 60000) {
      if (wifiConnect()) {
        
      }
      lastWifiMillis = curMillis;
    }
  }


// Handle MQTT
  static unsigned long lastMqttConnMillis = 0;
  if (wifiIsConnected) {
    if (!mqttClient.connected()) {
      if (curMillis - lastMqttConnMillis > 5000) {
        Serial.println(F("Disconnected from MQTT"));
        mqttConnect(true);
        lastMqttConnMillis = curMillis;
      }
    } else {
      mqttClient.loop();
    }
  }


// HANDLE MP3
  if (newAudioSource) {
    playAudio();
  } else if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      stopPlaying();
    }
  }
}
