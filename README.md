# WAM NFC Audio Box

An NFC-controlled audio player designed for the Western Australian Museum. This device reads NFC tags containing text records and plays corresponding audio files from an SD card.

## Overview

The NFC Audio Box allows visitors to tap NFC tags to hear associated audio content. When an NTAG213 tag is scanned, the device reads the text record, searches for a matching WAV file on the SD card, and plays the audio through connected speakers or headphones.

## Hardware Requirements

### Core Components
- **Teensy 4.0** (or compatible) microcontroller
- **Teensy Audio Shield** for audio output
- **Adafruit PN532 NFC Breakout Board** for NFC tag reading
- **MicroSD card** for audio file storage
- **Speakers or headphones** for audio output

### Pin Connections

#### PN532 NFC Reader
- IRQ: Pin 18
- RESET: Pin 19

#### SD Card (via Audio Shield)
- CS: Pin 10
- MOSI: Pin 7
- SCK: Pin 14

#### Audio Output
- Connected via Teensy Audio Shield I2S interface

## Software Dependencies

### Arduino Libraries
- `Adafruit_PN532` - NFC tag reading
- `Audio` - Teensy Audio Library
- `SD` - SD card file system
- `SerialFlash` - Flash memory support
- `EEPROM` - Volume persistence
- `Wire` - I2C communication
- `SPI` - SPI communication

Install these libraries through the Arduino Library Manager.

## Setup Instructions

### 1. Hardware Assembly
1. Connect the PN532 NFC breakout to the Teensy using I2C
2. Attach the Teensy Audio Shield
3. Insert a formatted microSD card into the Audio Shield
4. Connect speakers or headphones to the audio output

### 2. Audio File Preparation
- Convert all audio files to **WAV format**
- Use **16-bit, 44.1kHz** sample rate for best compatibility
- Name files using **UPPERCASE** letters (e.g., `HELLO.WAV`)
- Keep filenames short (under 27 characters including `.WAV`)

### 3. NFC Tag Programming
- Use NTAG213 tags
- Program tags with **text records** containing the filename (without `.WAV` extension)
- Text should match the WAV filename exactly (case-insensitive)

### 4. Upload Firmware
1. Open `WAM_NFC_Audio_Box.ino` in Arduino IDE
2. Select the appropriate Teensy board
3. Upload the sketch to your Teensy

## Usage

### Basic Operation
1. Power on the device
2. Wait for the startup sound (if `STARTUP.WAV` exists)
3. Tap an NFC tag near the PN532 reader
4. The corresponding audio file will play automatically

### Volume Control
The device supports persistent volume control via special NFC tags:

- **Volume Up**: Program a tag with text `VOLUMEUP`
- **Volume Down**: Program a tag with text `VOLUMEDN`

Volume settings are automatically saved to EEPROM and restored on power-up.

### Audio Feedback Files

Place these optional WAV files on the SD card for enhanced user feedback:

| Filename | Purpose |
|----------|---------|
| `STARTUP.WAV` | Played when device boots up |
| `NOMATCH.WAV` | Played when no matching audio file is found |
| `VOLUMEUP.WAV` | Confirmation sound for volume increase |
| `VOLUMEDN.WAV` | Confirmation sound for volume decrease |
| `VOLUMEMX.WAV` | Played when already at maximum volume |
| `VOLUMEMN.WAV` | Played when already at minimum volume |

## File Structure

```
SD Card Root/
├── STARTUP.WAV          # Optional: Boot sound
├── NOMATCH.WAV          # Optional: No match found sound
├── VOLUMEUP.WAV         # Optional: Volume up confirmation
├── VOLUMEDN.WAV         # Optional: Volume down confirmation
├── VOLUMEMX.WAV         # Optional: Maximum volume warning
├── VOLUMEMN.WAV         # Optional: Minimum volume warning
├── [CONTENT1].WAV       # Your audio content files
├── [CONTENT2].WAV       # Name these to match your NFC tags
└── ...
```

## Configuration

### Volume Settings
- **Default Volume**: 50%
- **Volume Range**: 10% - 100%
- **Volume Steps**: 10% increments
- **Hardware Codec Level**: 80%

### Performance Optimizations
- Serial output minimized for faster NFC reading
- Only reads necessary NFC tag pages (4-9) for short text records
- Efficient NDEF parsing for quick response times

## Troubleshooting

### Common Issues

**NFC tags not being read:**
- Ensure tags are NTAG213 compatible
- Check PN532 wiring connections
- Verify tags contain valid text records

**Audio not playing:**
- Check SD card formatting (FAT32 recommended)
- Verify WAV file format (16-bit, 44.1kHz)
- Ensure filenames match NFC tag text exactly
- Check audio shield connections

**Volume issues:**
- Try volume control tags (`VOLUMEUP`/`VOLUMEDN`)
- Check EEPROM settings
- Verify audio shield codec configuration

### Debug Mode
Uncomment debug lines in the code to enable serial output for troubleshooting.

## Technical Details

### Supported NFC Tags
- NTAG203, NTAG213, NTAG215, NTAG216
- Must contain NDEF text records
- 7-byte UID required

### Audio Specifications
- Format: WAV
- Sample Rate: 44.1kHz
- Bit Depth: 16-bit
- Channels: Mono or Stereo

### Memory Usage
- Audio memory allocation: 8 blocks
- EEPROM: 1 byte for volume storage
- RAM: Optimized for minimal NFC data buffering

## Author

Steve Berrick <steve@berrick.net>  
September 2025

## License

Created for the Western Australian Museum.
