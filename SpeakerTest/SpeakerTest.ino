/**************************************************************************/
/*
  WAM NFC Audio Box – Speaker Channel Test
  Steve Berrick <steve@berrick.net>

  Cycles through:
    1. LEFT  channel only  (plays STARTUP.WAV)
    2. RIGHT channel only  (plays STARTUP.WAV)
    3. BOTH  channels      (plays STARTUP.WAV)

  Prints status to Serial (115200 baud) so you can follow along.

  Uses STARTUP.WAV which is already present on the SD card.

  Hardware: Teensy + Teensy Audio Shield (same as main sketch)
*/
/**************************************************************************/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// ---------------------------------------------------------------------------
// Audio chain:
//   playSdWav1 (L) → mixer1 (gain controls L output) → i2s1 L
//   playSdWav1 (R) → mixer2 (gain controls R output) → i2s1 R
// ---------------------------------------------------------------------------
AudioPlaySdWav        playSdWav1;
AudioMixer4           mixer1;         // left
AudioMixer4           mixer2;         // right
AudioOutputI2S        i2s1;
AudioControlSGTL5000  sgtl5000_1;

AudioConnection  pc1(playSdWav1, 0, mixer1, 0);   // WAV left  → mixer L
AudioConnection  pc2(playSdWav1, 1, mixer2, 0);   // WAV right → mixer R
AudioConnection  pc3(mixer1, 0, i2s1, 0);         // mixer L   → I2S L
AudioConnection  pc4(mixer2, 0, i2s1, 1);         // mixer R   → I2S R

#define SDCARD_CS_PIN   10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN  14

#define TEST_FILE "STARTUP.WAV"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void setChannels(float leftGain, float rightGain) {
  mixer1.gain(0, leftGain);
  mixer2.gain(0, rightGain);
}

void playAndWait() {
  playSdWav1.play(TEST_FILE);
  delay(10);
  unsigned long start = millis();
  while (playSdWav1.isPlaying()) {
    delay(50);
    if (millis() - start > 10000) { // safety timeout
      playSdWav1.stop();
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("=== WAM Speaker Channel Test ===");

  AudioMemory(12);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8);
  setChannels(0.0, 0.0);

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  if (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("ERROR: SD card not found. Check card and wiring.");
    while (1);
  }
  Serial.println("SD card OK.");

  if (!SD.exists(TEST_FILE)) {
    Serial.println("ERROR: " TEST_FILE " not found on SD card.");
    while (1);
  }
  Serial.println("Test file: " TEST_FILE);
  Serial.println();
  Serial.println("Cycling:  LEFT  ->  RIGHT  ->  BOTH  (repeat)");
  Serial.println();
  delay(1000);
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
  Serial.println(">>> LEFT channel only");
  setChannels(1.0, 0.0);
  playAndWait();
  delay(600);

  Serial.println(">>> RIGHT channel only");
  setChannels(0.0, 1.0);
  playAndWait();
  delay(600);

  Serial.println(">>> BOTH channels");
  setChannels(1.0, 1.0);
  playAndWait();
  delay(1500);

  Serial.println("--- repeating ---");
  Serial.println();
}
