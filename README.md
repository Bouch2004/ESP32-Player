# ESP32-S3 USB Audio Sampler

A robust, standalone USB Audio Class (UAC) sampler firmware for the ESP32-S3.

This project turns an ESP32-S3 DevKitC into a hardware sampler. It decodes up to 3 separate MP3 files completely into PSRAM at boot time, and then streams the raw uncompressed audio directly to a PC over the native USB port. This architecture eliminates CPU bottlenecks and guarantees zero-latency, drop-free audio playback.

## Features

- **Pre-Decode Architecture**: MP3 files are decoded completely into PSRAM before the USB connection is initialized, ensuring 100% stable UAC streaming without CPU starvation.
- **Dynamic Memory Allocation**: Automatically allocates the exact PSRAM needed based on the embedded MP3 file size (safe up to the 8MB limit).
- **Multi-Sound Engine**: Supports 3 distinct sound files mapped to 3 hardware inputs.
- **Smart Interrupt Logic**: 
  - Tap a new button to play a sound.
  - Tap the *same* button to instantly re-trigger the sound.
  - To interrupt a currently playing sound with a *different* sound, **double-tap** the new button within 500ms (prevents accidental interruptions).

## Hardware Setup

1. **Board**: ESP32-S3 DevKitC (Requires a version with 8MB Octal PSRAM).
2. **USB Connections**: 
   - **UART Port**: Used for flashing and serial monitoring.
   - **USB/OTG Port**: Connects to the PC to act as the USB Audio device. *(Both must be plugged in during development).*
3. **Inputs**:
   - `GPIO 0` (BOOT Button): Triggers Sound 1.
   - `GPIO 1` (Touch Pad): Triggers Sound 2.
   - `GPIO 2` (Touch Pad): Triggers Sound 3.

*See `wiring.md` for full connection details.*

## How to Add Your Own Sounds

The firmware comes with placeholder MP3 files. To use your own audio:

1. Prepare three `.mp3` files. **Important:** They must be encoded at exactly **44,100 Hz** (Stereo or Mono) to match the UAC output sample rate.
2. Name them `sound1.mp3`, `sound2.mp3`, and `sound3.mp3`.
3. Place them in the `firmware/main/sounds/` directory, overwriting the placeholders.
4. Rebuild and flash the firmware.

## Build and Flash

This project uses the standard ESP-IDF (v5.3+) toolchain. A convenience script `manage.sh` is provided in the repository root.

```bash
# To build the firmware
./manage.sh build firmware

# To flash the firmware (ensure UART USB is connected)
./manage.sh flash firmware

# To view the serial monitor
./manage.sh monitor firmware
```

## System Requirements
- ESP-IDF v5.3 or newer
- Linux/Ubuntu host for development
- Target must be `esp32s3` with `CONFIG_SPIRAM=y` and `CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768` configured (handled in `sdkconfig.defaults.esp32s3`).
