#include <Arduino.h>

#ifdef ESP32
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"


// You may need a fast SD card. Set this as high as it will work (40MHz max).
#define SPI_SPEED SD_SCK_MHZ(40)

// On ESP32 you can adjust the SPDIF_OUT_PIN (GPIO number).
// On ESP8266 it is fixed to GPIO3/RX0 and this setting has no effect

File dir;
AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;
AudioGeneratorMP3 *decoder = NULL;

String dirname = "/mp3/Lakatos/0007-hosi-to-je-neuveritelne.mp3";
//String dirname = "/";

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(1000);

  audioLogger = &Serial;
  source = new AudioFileSourceSD();
  output = new AudioOutputI2S();
  decoder = new AudioGeneratorMP3();

  // NOTE: SD.begin(...) should be called AFTER AudioOutputSPDIF()
  //       to takover the the SPI pins if they share some with I2S
  //       (i.e. D8 on Wemos D1 mini is both I2S BCK and SPI SS)
#if defined(ESP8266)
  SD.begin(0, SPI_SPEED);
#else
  SD.begin();
#endif
  dir = SD.open(dirname);
}

void loop() {
  if ((decoder) && (decoder->isRunning())) {
    if (!decoder->loop()) decoder->stop();
  } else {
    File file = dir.openNextFile();
    if (file) {
      if (String(file.name()).endsWith(".mp3")) {
        source->close();
        Serial.println(file.name());
        const char* dirnameC = dirname.c_str();
        if (source->open(dirnameC)) {
          //        if (source->open("/mp3/Lakatos/0007-hosi-to-je-neuveritelne.mp3")) {
          Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), strcpy("/mp3/Lakatos/", file.name()));
          decoder->begin(source, output);
        } else {
          Serial.printf_P(PSTR("Error opening '%s'\n"), file.name());
        }
      }
    }
  }
}
