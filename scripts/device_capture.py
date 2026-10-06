#!/usr/bin/env python3
"""Reads one whole report of the on-chip bench from a serial port.

    device_capture.py PORT LOG [TIMEOUT_SECONDS]

The bench repeats its report; this waits for a BENCH BEGIN, copies every line to stdout and LOG
through DEVICE DONE, and exits 0 if the block said DEVICE PASS. DTR and RTS are held low so opening
the port neither resets the chip nor sends it to its bootloader. Needs pyserial (the ESP-IDF image)."""
import sys
import time

import serial

port, log = sys.argv[1], sys.argv[2]
limit = float(sys.argv[3]) if len(sys.argv) > 3 else 600
start = time.time()
s = None
while s is None and time.time() - start < limit:
    try:                                    # the port comes back a moment after the flash's reset
        s = serial.Serial()
        s.port, s.baudrate, s.timeout = port, 115200, 0.5
        s.dtr = s.rts = False
        s.open()
    except serial.SerialException:
        s = None
        time.sleep(0.5)
buf, block, verdict = b"", None, 1
with open(log, "w") as out:
    while s and time.time() - start < limit:
        buf += s.read(4096)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode("utf-8", "replace").rstrip("\r")
            if line.startswith("BENCH BEGIN"):
                block = []
            if "Guru Meditation" in line or "panic" in line or line.startswith(("Backtrace:", "Debug exception")):
                # a crash is the report: print it and stop, rather than wait out the timeout
                print(line, flush=True)
                out.write(line + "\n")
                more = s.read(2048).decode("utf-8", "replace")
                print(more, flush=True)
                out.write(more)
                print("device_capture.py: the chip panicked", flush=True)
                sys.exit(1)
            if block is None:
                continue
            block.append(line)
            print(line, flush=True)
            out.write(line + "\n")
            if line.startswith("DEVICE PASS"):
                verdict = 0
            if line.startswith("DEVICE DONE"):
                sys.exit(verdict)
print(f"device_capture.py: no whole report within {limit:.0f} s", flush=True)
sys.exit(1)
