# Wiring

## Touch Inputs

| Pad | GPIO | Wire colour (suggestion) |
|---|---|---|
| Touch Pad 1 | GPIO 1 | Yellow |
| Touch Pad 2 | GPIO 2 | Blue |

Expose a short (~10 cm) bare wire from each GPIO pin. The touch is detected capacitively — no extra components needed.

## USB

Connect the **native USB port** (not the UART/debug port) of the ESP32-S3 DevKitC to the host PC.

## Power

The board can be powered via either USB port. If using the native USB port for audio, power through the UART port or an external 3.3 V supply.
