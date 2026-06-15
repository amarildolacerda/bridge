#!/usr/bin/env python3
"""Monitor serial do Bridge ESP32.
Encerra com 'q' ou Ctrl+C.
"""

import sys
import tty
import termios
import select
import serial
import argparse
import threading

def main():
    parser = argparse.ArgumentParser(description="Monitor serial ESP32 Bridge")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0",
                        help="Porta serial (default: /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)

    print(f"Conectado: {args.port} @ {args.baud}")
    print("Para sair: 'q' ou Ctrl+C")
    print("-" * 40)

    try:
        tty.setraw(fd)

        while True:
            r, _, _ = select.select([fd, ser.fd], [], [], 0.1)

            for f in r:
                if f == fd:
                    data = sys.stdin.buffer.read(1)
                    if not data:
                        continue
                    if data in (b'q', b'Q'):
                        print()
                        return
                    ser.write(data)
                elif f == ser.fd:
                    data = ser.read(ser.in_waiting or 1)
                    if data:
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
    except KeyboardInterrupt:
        print()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        ser.close()

if __name__ == "__main__":
    main()
