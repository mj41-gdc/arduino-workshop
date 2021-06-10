/*
  Listfiles

  This example shows how print out the files in a
  directory on a SD card

  The circuit:
   SD card attached to SPI bus as follows:
 ** MOSI - D7
 ** MISO - D6
 ** CLK - D5
 ** CS - D3

  created   Nov 2010
  by David A. Mellis
  modified 9 Apr 2012
  by Tom Igoe
  modified 2 Feb 2014
  by Scott Fitzgerald

  This example code is in the public domain.

*/
#include <SPI.h>
#include <SD.h>

#define MAXDIRDEPTH 4
#define ROOT_FOLDER "/"
File root;

String dirs[MAXDIRDEPTH];
String filepath=ROOT_FOLDER;
String files[256];
uint8_t numfiles=0;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  Serial.print("Initializing SD card...");

  if (!SD.begin(0)) { //0 means GPIO0 = D3 (changed from default GPIO15 D8)
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  root = SD.open(filepath);

  readDirectory(root, 0);

  Serial.println("done!");
}

void loop() {
  for (uint8_t i = 0; i < numfiles; i++) {
    Serial.println(files[i]);
  }
  delay(5000);
}

void readDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      filepath=filepath+dirs[i]+"/";
//      Serial.print(dirs[i]);
//      Serial.print("/");
    }
//    Serial.print(entry.name());
    filepath=filepath+entry.name();
    if (entry.isDirectory()) {
//      Serial.println("/");
      filepath = ROOT_FOLDER;
      dirs[numTabs] = entry.name();
      readDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      //Serial.print("\t\t");
//      Serial.print(entry.size(), DEC);
      //time_t cr = entry.getCreationTime();
      //time_t lw = entry.getLastWrite();
      //struct tm * tmstruct = localtime(&cr);
      //Serial.printf("\tCREATION: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
      //tmstruct = localtime(&lw);
      //Serial.printf("\tLAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
//      Serial.printf("\n");
//      Serial.print("soubor: ");
//      Serial.println(filepath);
      files[numfiles]=filepath;
      numfiles++;
      filepath = ROOT_FOLDER;
    }
    entry.close();
  }
}
