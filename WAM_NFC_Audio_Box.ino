/**************************************************************************/
/*
  WA Museum NFC Audio Box
  Steve Berrick <steve@berrick.net>
  September 2025

  Reads NTAG213 NFC tags using an Adafruit PN532 Breakout
  Checks for a text record, then checks the SD card for a corresponding .wav file
  Plays the wav file

  Audio chain:
    playSdWav1 → mixer1 → eqLeft  → i2s1 (L)
    playSdWav1 → mixer2 → eqRight → i2s1 (R)

  Each AudioFilterBiquad has 4 stages used as follows:
    Stage 0 – Low shelf  cut   (-3 dB @ 300 Hz)  – tames box resonance
    Stage 1 – Peaking EQ boost (+4 dB @ 2 kHz)   – vocal presence
    Stage 2 – High shelf boost (+2 dB @ 6 kHz)   – air / clarity
    Stage 3 – Pass-through (unused / identity)

*/
/**************************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Audio.h>
#include <SD.h>
#include <SerialFlash.h>
#include <EEPROM.h>

// PN532 setup
#define PN532_IRQ   18
#define PN532_RESET 19  // Not connected by default on the NFC Shield

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Teensy Audio Shield setup
AudioPlaySdWav           playSdWav1;
AudioMixer4              mixer1;
AudioMixer4              mixer2;
AudioFilterBiquad        eqLeft;
AudioFilterBiquad        eqRight;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playSdWav1, 1, mixer2, 0);
AudioConnection          patchCord3(mixer1, 0, eqLeft, 0);
AudioConnection          patchCord4(mixer2, 0, eqRight, 0);
AudioConnection          patchCord5(eqLeft, 0, i2s1, 0);
AudioConnection          patchCord6(eqRight, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

// ---------------------------------------------------------------------------
// EEPROM addresses
// ---------------------------------------------------------------------------
const int EEPROM_VOLUME_ADDRESS = 0; // 1 byte: volume 0-100
const int EEPROM_EQ_ADDRESS     = 1; // 1 byte: 0 = EQ off, 1 = EQ on

// ---------------------------------------------------------------------------
// Volume control
// ---------------------------------------------------------------------------
float currentVolume = 0.5; // Default 50%, loaded from EEPROM
const float volumeStep = 0.1;
const float minVolume  = 0.1;
const float maxVolume  = 1.0;

void saveVolumeToEEPROM() {
  EEPROM.write(EEPROM_VOLUME_ADDRESS, (int)(currentVolume * 100));
}

void loadVolumeFromEEPROM() {
  int v = EEPROM.read(EEPROM_VOLUME_ADDRESS);
  if (v <= 100) {
    currentVolume = v / 100.0;
  } else {
    currentVolume = 0.5;
    saveVolumeToEEPROM();
  }
}

void volumeUp() {
  if (currentVolume >= maxVolume) {
    if (SD.exists("VOLUMEMX.WAV")) {
      playSdWav1.play("VOLUMEMX.WAV");
      delay(10);
      while (playSdWav1.isPlaying()) delay(50);
    }
    return;
  }
  currentVolume += volumeStep;
  if (currentVolume > maxVolume) currentVolume = maxVolume;
  mixer1.gain(0, currentVolume);
  mixer2.gain(0, currentVolume);
  saveVolumeToEEPROM();
  if (SD.exists("VOLUMEUP.WAV")) {
    playSdWav1.play("VOLUMEUP.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) delay(50);
  }
}

void volumeDown() {
  if (currentVolume <= minVolume) {
    if (SD.exists("VOLUMEMN.WAV")) {
      playSdWav1.play("VOLUMEMN.WAV");
      delay(10);
      while (playSdWav1.isPlaying()) delay(50);
    }
    return;
  }
  currentVolume -= volumeStep;
  if (currentVolume < minVolume) currentVolume = minVolume;
  mixer1.gain(0, currentVolume);
  mixer2.gain(0, currentVolume);
  saveVolumeToEEPROM();
  if (SD.exists("VOLUMEDN.WAV")) {
    playSdWav1.play("VOLUMEDN.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) delay(50);
  }
}

// ---------------------------------------------------------------------------
// EQ (AudioFilterBiquad) helpers
// ---------------------------------------------------------------------------
bool eqEnabled = false; // Runtime state, loaded from EEPROM

/*
  Two presets: flat (0) and vocal boost (1).

  Vocal-boost curve:
    Stage 0 – Low shelf cut  : 400 Hz, −3 dB, slope 0.707
    Stage 1 – Peaking bell   : 2500 Hz, +6 dB, Q=1.4
    Stage 2 – High shelf lift: 5000 Hz, +4 dB, slope 0.707

  Peaking coefficients pre-computed via Audio EQ Cookbook (Bristow-Johnson):
    w0=2π*2500/44100=0.35700, A=10^(6/40)=1.41254
    alpha=sin(w0)/(2*Q)=0.12533
    Normalised (divided by a0): b0 b1 b2 a1 a2
*/
void applyEQSettings(bool enabled) {
  // Identity biquad (pass-through) for stage 1 when flat
  static const double peakFlat[5]  = { 1.0, 0.0, 0.0, 0.0, 0.0 };

  // +6 dB @ 2500 Hz, Q=1.4
  static const double peakBoost[5] = { 1.08110, -1.72009, 0.75597, -1.72009, 0.83700 };

  if (enabled) {
    Serial.println("EQ: VOCAL BOOST (shelf -3dB/400Hz, peak +6dB/2500Hz, treble +4dB/5kHz)");
    eqLeft.setLowShelf( 0, 400, -3.0, 0.707);
    eqRight.setLowShelf(0, 400, -3.0, 0.707);
    eqLeft.setCoefficients( 1, peakBoost);
    eqRight.setCoefficients(1, peakBoost);
    eqLeft.setHighShelf( 2, 5000, 4.0, 0.707);
    eqRight.setHighShelf(2, 5000, 4.0, 0.707);
  } else {
    Serial.println("EQ: FLAT (bypass)");
    eqLeft.setLowShelf( 0, 400, 0.0, 0.707);
    eqRight.setLowShelf(0, 400, 0.0, 0.707);
    eqLeft.setCoefficients( 1, peakFlat);
    eqRight.setCoefficients(1, peakFlat);
    eqLeft.setHighShelf( 2, 5000, 0.0, 0.707);
    eqRight.setHighShelf(2, 5000, 0.0, 0.707);
  }
}

void saveEQToEEPROM() {
  EEPROM.write(EEPROM_EQ_ADDRESS, eqEnabled ? 1 : 0);
}

void loadEQFromEEPROM() {
  uint8_t val = EEPROM.read(EEPROM_EQ_ADDRESS);
  eqEnabled = (val == 1);
  applyEQSettings(eqEnabled);
}

void eqOn() {
  eqEnabled = true;
  applyEQSettings(true);
  saveEQToEEPROM();
  Serial.println("EQ ON");
}

void eqOff() {
  eqEnabled = false;
  applyEQSettings(false);
  saveEQToEEPROM();
  Serial.println("EQ OFF");
}

// ---------------------------------------------------------------------------
// A/B test – plays a WAV file flat then with vocal-boost EQ
// ---------------------------------------------------------------------------
void playABTest(const char* filename) {
  Serial.print("A/B test: ");
  Serial.println(filename);

  // --- A: flat ---
  Serial.println("  [A] Flat");
  applyEQSettings(false);
  playSdWav1.play(filename);
  delay(10);
  while (playSdWav1.isPlaying()) delay(100);

  delay(700); // pause between A and B

  // --- B: vocal boost ---
  Serial.println("  [B] Vocal boost");
  applyEQSettings(true);
  playSdWav1.play(filename);
  delay(10);
  while (playSdWav1.isPlaying()) delay(100);

  // Restore saved EQ state
  applyEQSettings(eqEnabled);
}

// ---------------------------------------------------------------------------
// NDEF Text Record parsing
// ---------------------------------------------------------------------------
bool parseNDEFTextRecord(uint8_t* data, int dataLength, char* textOutput, int maxTextLength) {
  for (int i = 0; i < dataLength - 6; i++) {
    if (data[i] == 0xD1 && data[i + 1] == 0x01 && data[i + 3] == 0x54) {
      uint8_t payloadLength = data[i + 2];
      uint8_t statusByte   = data[i + 4];
      uint8_t langCodeLength = statusByte & 0x3F;

      int textStart  = i + 5 + langCodeLength;
      int textLength = payloadLength - 1 - langCodeLength;

      if (textLength > 0 && textLength < maxTextLength && textStart + textLength <= dataLength) {
        memcpy(textOutput, &data[textStart], textLength);
        textOutput[textLength] = '\0';
        return true;
      }
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// WAV playback
// ---------------------------------------------------------------------------

// Track the last successfully played filename for A/B testing
char lastPlayedFile[13] = "";

// Alternating file counter (1 or 2)
int currentFileNumber = 1;

// Function to search for WAV file on SD card and play it.
// Alternates between file1.WAV and file2.WAV for each trigger word.
// Enforces 8.3 filename convention: max 7 chars + 1 number + .WAV
bool searchAndPlayWAV(char* textBuffer) {
  char baseFilename[8];
  int len = strlen(textBuffer);
  if (len > 7) len = 7;

  for (int i = 0; i < len; i++) {
    baseFilename[i] = toupper(textBuffer[i]);
  }
  baseFilename[len] = '\0';

  char filename[13];
  bool fileFound = false;
  int fileNumberToPlay = currentFileNumber;

  sprintf(filename, "%s%d.WAV", baseFilename, fileNumberToPlay);
  if (SD.exists(filename)) {
    fileFound = true;
  } else {
    fileNumberToPlay = (fileNumberToPlay == 1) ? 2 : 1;
    sprintf(filename, "%s%d.WAV", baseFilename, fileNumberToPlay);
    if (SD.exists(filename)) {
      fileFound = true;
    } else {
      sprintf(filename, "%s.WAV", baseFilename);
      if (SD.exists(filename)) {
        fileFound = true;
        fileNumberToPlay = 0;
      }
    }
  }

  if (fileFound) {
    // Remember this file so ABTEST can re-use it
    strncpy(lastPlayedFile, filename, sizeof(lastPlayedFile) - 1);
    lastPlayedFile[sizeof(lastPlayedFile) - 1] = '\0';

    playSdWav1.play(filename);
    delay(10);
    while (playSdWav1.isPlaying()) delay(100);

    if (fileNumberToPlay != 0) {
      currentFileNumber = (currentFileNumber == 1) ? 2 : 1;
    }
    return true;
  }
  return false;
}

void playNoMatch() {
  if (SD.exists("NOMATCH.WAV")) {
    playSdWav1.play("NOMATCH.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) delay(100);
  }
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup(void) {
  Serial.begin(115200);
  Serial.println("WAM NFC Audio Box");

  loadVolumeFromEEPROM();

  AudioMemory(12);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8);

  mixer1.gain(0, currentVolume);
  mixer2.gain(0, currentVolume);

  // Load and apply EQ state from EEPROM
  loadEQFromEEPROM();
  Serial.print("EQ state: ");
  Serial.println(eqEnabled ? "ON" : "OFF");

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    Serial.println("Unable to access the SD card");
    while (1);
  }
  Serial.println("SD card initialized");

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1);
  }
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  if (SD.exists("STARTUP.WAV")) {
    playSdWav1.play("STARTUP.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) delay(100);
  }

  Serial.println("Setup complete");
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop(void) {
  uint8_t success;
  uint8_t uid[]     = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    if (uidLength == 7) {
      uint8_t data[32];
      uint8_t allData[24];
      int dataIndex = 0;
      bool readSuccess = true;

      for (uint8_t i = 4; i < 10; i++) {
        success = nfc.ntag2xx_ReadPage(i, data);
        if (success) {
          if (dataIndex + 4 <= 24) {
            memcpy(&allData[dataIndex], data, 4);
            dataIndex += 4;
          }
        } else {
          readSuccess = false;
          break;
        }
      }

      if (readSuccess && dataIndex > 0) {
        char textBuffer[128];
        if (parseNDEFTextRecord(allData, dataIndex, textBuffer, sizeof(textBuffer))) {

          // Uppercase normalisation
          for (int i = 0; textBuffer[i]; i++) {
            textBuffer[i] = toupper(textBuffer[i]);
          }

          // ---------- Special commands ----------
          if (strcmp(textBuffer, "VOLUMEUP") == 0) {
            volumeUp();

          } else if (strcmp(textBuffer, "VOLUMEDN") == 0) {
            volumeDown();

          } else if (strcmp(textBuffer, "EQON") == 0) {
            eqOn();

          } else if (strcmp(textBuffer, "EQOFF") == 0) {
            eqOff();

          } else if (strcmp(textBuffer, "ABTEST") == 0) {
            // A/B test using the last played file
            if (lastPlayedFile[0] != '\0') {
              playABTest(lastPlayedFile);
            } else if (SD.exists("ABTEST.WAV")) {
              // Fallback: dedicated test file
              playABTest("ABTEST.WAV");
            } else {
              playNoMatch();
            }

          } else {
            // ---------- Regular WAV playback ----------
            if (!searchAndPlayWAV(textBuffer)) {
              playNoMatch();
            }
          }
        }
      }
    } else {
      Serial.println("Not an NTAG203 tag (UID length != 7 bytes)");
    }

    delay(100);
  }
}
