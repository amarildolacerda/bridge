#!/bin/bash
set -e
cd "$(dirname "$0")"
source config.sh
PORT="${1:-/dev/ttyUSB0}"
idf.py erase-flash -p "$PORT"
