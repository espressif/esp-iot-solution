# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Read the test console without toggling reset/boot. Does not prove visual output."""
import argparse
import time
import serial

p = argparse.ArgumentParser()
p.add_argument('port')
p.add_argument('output')
p.add_argument('--seconds', type=float, default=45)
a = p.parse_args()
s = serial.Serial(port=None, baudrate=115200, timeout=0.2)
s.dtr = False
s.rts = False
s.port = a.port
s.open()
try:
    with open(a.output, 'wb') as f:
        end = time.monotonic() + a.seconds
        while time.monotonic() < end:
            data = s.read(s.in_waiting or 1)
            if data:
                f.write(data)
                f.flush()
except KeyboardInterrupt:
    pass
finally:
    s.close()
