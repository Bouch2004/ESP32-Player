#!/usr/bin/env bash

set -e

if [ -z "$IDF_PATH" ]; then
    IDF_EXPORT="$HOME/esp/esp-idf/export.sh"
    if [ -f "$IDF_EXPORT" ]; then
        echo "Sourcing ESP-IDF from $IDF_EXPORT..."
        . "$IDF_EXPORT"
    else
        echo "Error: ESP-IDF not found at $IDF_EXPORT"
        echo "Set IDF_PATH or install ESP-IDF first."
        exit 1
    fi
fi

FIRMWARE_DIR="$(dirname "$0")/../firmware"

usage() {
    echo "Usage: $0 [build|flash|monitor|flash-monitor|clean]"
    exit 1
}

PORT="/dev/ttyACM0"
if [ ! -c "$PORT" ]; then PORT="/dev/ttyUSB0"; fi
if [ ! -c "$PORT" ]; then
    echo "Warning: No serial device found on /dev/ttyACM0 or /dev/ttyUSB0"
    PORT=""
fi

ACTION="${1:-flash-monitor}"

echo "=================================================="
echo "Firmware: $FIRMWARE_DIR"
echo "Action:   $ACTION"
[ -n "$PORT" ] && echo "Port:     $PORT"
echo "=================================================="

case "$ACTION" in
    build)
        idf.py -C "$FIRMWARE_DIR" build
        ;;
    flash)
        [ -z "$PORT" ] && echo "Error: No device found." && exit 1
        idf.py -C "$FIRMWARE_DIR" -p "$PORT" flash
        ;;
    monitor)
        [ -z "$PORT" ] && echo "Error: No device found." && exit 1
        idf.py -C "$FIRMWARE_DIR" -p "$PORT" monitor
        ;;
    flash-monitor)
        [ -z "$PORT" ] && echo "Error: No device found." && exit 1
        idf.py -C "$FIRMWARE_DIR" -p "$PORT" flash monitor
        ;;
    clean)
        idf.py -C "$FIRMWARE_DIR" fullclean
        ;;
    *)
        usage
        ;;
esac
