#!/bin/bash
set -e
command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }
pio device monitor
