# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Capture an identified board; exit on suite completion or a firmware panic."""
import argparse
from pathlib import Path
import time
import serial
from esptool.reset import HardReset


def capture(port, output, usb, seconds):
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    complete = False
    try:
        connection.reset_input_buffer()
        HardReset(connection, uses_usb=usb)()
        deadline = time.monotonic() + seconds
        tail = b''
        with output.open('wb') as stream:
            while time.monotonic() < deadline:
                data = connection.read(connection.in_waiting or 1)
                if not data:
                    continue
                stream.write(data)
                stream.flush()
                tail = (tail + data)[-4096:]
                if b'BOUNDARY_SUITE_COMPLETE' in tail:
                    complete = True
                    break
                if any(marker in tail for marker in (b'Guru Meditation', b'assert failed:', b'abort() was called', b'Task watchdog got triggered')):
                    break
    finally:
        connection.close()
    return complete


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('output', type=Path)
    parser.add_argument('--usb', action='store_true', help='USB Serial/JTAG reset; omit for the S3 CP2102 UART')
    parser.add_argument('--seconds', type=float, default=240)
    args = parser.parse_args()
    complete = capture(args.port, args.output, args.usb, args.seconds)
    print('capture complete' if complete else 'capture incomplete')
    raise SystemExit(0 if complete else 1)
