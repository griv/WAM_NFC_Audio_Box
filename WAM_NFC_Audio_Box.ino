/**************************************************************************/
/*
  WA Museum NFC Audio Box
  Steve Berrick <steve@berrick.net>
  September 2025

  Reads NTAG213 NFC tags using an Adafruit PN532 Breakout
  Checks for a text record, then checks the SD card for a corresponding .wav file
  Plays the wav file

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
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playSdWav1, 1, mixer2, 0);
AudioConnection          patchCord3(mixer1, 0, i2s1, 0);
AudioConnection          patchCord4(mixer2, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;

#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

// Volume control
float currentVolume = 0.5; // Default 50% volume, will be loaded from EEPROM
const float volumeStep = 0.1; // 10% steps
const float minVolume = 0.1;
const float maxVolume = 1.0;
const int EEPROM_VOLUME_ADDRESS = 0; // EEPROM address to store volume

// Save volume to EEPROM
void saveVolumeToEEPROM() {
  int volumeInt = (int)(currentVolume * 100); // Convert to 0-100 integer
  EEPROM.write(EEPROM_VOLUME_ADDRESS, volumeInt);
}

// Load volume from EEPROM
void loadVolumeFromEEPROM() {
  int volumeInt = EEPROM.read(EEPROM_VOLUME_ADDRESS);
  if (volumeInt <= 100) { // Valid range check
    currentVolume = volumeInt / 100.0; // Convert back to float 0.0-1.0
  } else {
    currentVolume = 0.5; // Default to 50% if invalid data
    saveVolumeToEEPROM(); // Save default value
  }
}

// Volume control functions
void volumeUp() {
  // Check if already at maximum volume
  if (currentVolume >= maxVolume) {
    // Already at max, play max volume sound
    if (SD.exists("VOLUMEMX.WAV")) {
      playSdWav1.play("VOLUMEMX.WAV");
      delay(10);
      while (playSdWav1.isPlaying()) {
        delay(50);
      }
    }
    return; // Don't change volume or save to EEPROM
  }
  
  // Increase volume
  currentVolume += volumeStep;
  if (currentVolume > maxVolume) {
    currentVolume = maxVolume;
  }
  // Set mixer gain for both channels
  mixer1.gain(0, currentVolume);
  mixer2.gain(0, currentVolume);
  saveVolumeToEEPROM(); // Persist volume change
  
  // Play confirmation beep if VOLUMEUP.WAV exists
  if (SD.exists("VOLUMEUP.WAV")) {
    playSdWav1.play("VOLUMEUP.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) {
      delay(50);
    }
  }
}

void volumeDown() {
  // Check if already at minimum volume
  if (currentVolume <= minVolume) {
    // Already at min, play min volume sound
    if (SD.exists("VOLUMEMN.WAV")) {
      playSdWav1.play("VOLUMEMN.WAV");
      delay(10);
      while (playSdWav1.isPlaying()) {
        delay(50);
      }
    }
    return; // Don't change volume or save to EEPROM
  }
  
  // Decrease volume
  currentVolume -= volumeStep;
  if (currentVolume < minVolume) {
    currentVolume = minVolume;
  }
  // Set mixer gain for both channels
  mixer1.gain(0, currentVolume);
  mixer2.gain(0, currentVolume);
  saveVolumeToEEPROM(); // Persist volume change
  
  // Play confirmation beep if VOLUMEDN.WAV exists
  if (SD.exists("VOLUMEDN.WAV")) {
    playSdWav1.play("VOLUMEDN.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) {
      delay(50);
    }
  }
}

// NDEF Text Record parsing functions
bool parseNDEFTextRecord(uint8_t* data, int dataLength, char* textOutput, int maxTextLength) {
  // Look for the NDEF Text Record pattern: D1 01 xx 54 xx
  // D1 = NDEF header for text record, 01 = type length, xx = payload length, 54 = 'T', xx = status
  
  for (int i = 0; i < dataLength - 6; i++) {
    if (data[i] == 0xD1 && data[i + 1] == 0x01 && data[i + 3] == 0x54) {
      // Found NDEF Text Record pattern
      uint8_t payloadLength = data[i + 2];
      uint8_t statusByte = data[i + 4];
      uint8_t langCodeLength = statusByte & 0x3F;
      
      // Calculate text start position and length
      int textStart = i + 5 + langCodeLength; // Skip header(3) + type(1) + status(1) + language code
      int textLength = payloadLength - 1 - langCodeLength; // Total payload minus status byte and language code
      
      // Verify we have enough data and space
      if (textLength > 0 && textLength < maxTextLength && textStart + textLength <= dataLength) {
        memcpy(textOutput, &data[textStart], textLength);
        textOutput[textLength] = '\0';
        
        // Debug output
        // Debug output removed for speed
        
        return true;
      }
    }
  }
  return false;
}

// Function to search for WAV file on SD card and play it
bool searchAndPlayWAV(char* textBuffer) {
  // Convert text buffer to uppercase for consistent matching
  char filename[32];
  int len = strlen(textBuffer);
  for (int i = 0; i < len && i < 27; i++) { // Leave room for ".WAV" + null terminator
    filename[i] = toupper(textBuffer[i]);
  }
  filename[len] = '\0';
  strcat(filename, ".WAV");
  
  // Serial output removed for speed
  
  // Check if file exists on SD card
  if (SD.exists(filename)) {
    // File found - play without serial output for speed
    
    playSdWav1.play(filename);
    
    // Wait for playback to start
    delay(10);
    
    // Wait for playback to finish
    while (playSdWav1.isPlaying()) {
      delay(100);
    }
    
    // Playback finished - no serial output for speed
    return true;
  } else {
    // File not found - no serial output for speed
    return false;
  }
}

// Function to play the "no match" sound
void playNoMatch() {
  // Playing NOMATCH.WAV - no serial output for speed
  if (SD.exists("NOMATCH.WAV")) {
    playSdWav1.play("NOMATCH.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) {
      delay(100);
    }
    // NOMATCH.WAV finished - no serial output for speed
  } else {
    // NOMATCH.WAV not found - no serial output for speed
  }
}

void setup(void) {
  
  Serial.begin(115200);
  while (!Serial) delay(10); // for Leonardo/Micro/Zero

  Serial.println("WAM NFC Audio Box");
  
  // Load saved volume from EEPROM
  loadVolumeFromEEPROM();
  
  // Initialize audio shield
  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8); // Set codec to 80% (hardware level)
  
  // Set initial mixer volume from EEPROM
  mixer1.gain(0, currentVolume);
  mixer2.gain(0, currentVolume);
  
  // Initialize SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    Serial.println("Unable to access the SD card");
    while (1); // halt
  }
  Serial.println("SD card initialized");

  // Initialize NFC
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

  // Play startup sound if STARTUP.WAV exists
  if (SD.exists("STARTUP.WAV")) {
    playSdWav1.play("STARTUP.WAV");
    delay(10);
    while (playSdWav1.isPlaying()) {
      delay(100);
    }
  }

  Serial.println("Setup complete");
}

void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UUID (normally 7)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    // Minimal serial output for speed - just process the card
    if (uidLength == 7)
    {
      uint8_t data[32];
      uint8_t allData[24]; // Buffer for minimal pages only (6 pages * 4 bytes)
      int dataIndex = 0;

      // NTAG2x3 cards have 39*4 bytes of user pages (156 user bytes),
      // starting at page 4 ... larger cards just add pages to the end of
      // this range:

      // See: http://www.nxp.com/documents/short_data_sheet/NTAG203_SDS.pdf

      // TAG Type       PAGES   USER START    USER STOP
      // --------       -----   ----------    ---------
      // NTAG 203       42      4             39
      // NTAG 213       45      4             39
      // NTAG 215       135     4             129
      // NTAG 216       231     4             225

      // Read minimal pages for short text records (8 chars max) - ultra fast
      bool readSuccess = true;
      for (uint8_t i = 4; i < 10; i++) // Pages 4-9 should contain short NDEF text records
      {
        success = nfc.ntag2xx_ReadPage(i, data);

        if (success)
        {
          // Store data in buffer for NDEF parsing
          if (dataIndex + 4 <= 24) { // Reduced buffer size for speed
            memcpy(&allData[dataIndex], data, 4);
            dataIndex += 4;
          }
        }
        else
        {
          readSuccess = false;
          break;
        }
      }

      // Parse NDEF data for text records - only act on successful complete reads
      if (readSuccess && dataIndex > 0) {
        char textBuffer[128];
        if (parseNDEFTextRecord(allData, dataIndex, textBuffer, sizeof(textBuffer))) {
          
          // Convert to uppercase for consistent comparison
          for (int i = 0; textBuffer[i]; i++) {
            textBuffer[i] = toupper(textBuffer[i]);
          }
          
          // Check for special volume control commands first
          if (strcmp(textBuffer, "VOLUMEUP") == 0) {
            volumeUp();
          } else if (strcmp(textBuffer, "VOLUMEDN") == 0) {
            volumeDown();
          } else {
            // Regular WAV file search and playback
            if (!searchAndPlayWAV(textBuffer)) {
              // If no matching file found, play NOMATCH.WAV
              playNoMatch();
            }
          }
        }
        // If parsing fails on a complete read, do nothing (silent)
      }
      // If read fails or incomplete, do nothing (silent) - don't play nomatch
    }
    else
    {
      Serial.println("This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
    }

    delay(100);

  }
}
