#!/bin/bash
# Compare the cooked and raw ioctl results for one Lunix device.
# Usage: ./lunix-ioctl-compare.sh [/dev/lunix0-temp]

set -eu

DEV="${1:-/dev/lunix0-temp}"

if [ ! -e "$DEV" ]; then
    echo "Device node not found: $DEV" >&2
    exit 1
fi

python3 - "$DEV" <<'PY'
import datetime
import fcntl
import math
import os
import struct
import sys

device = sys.argv[1]

# The ioctl numbers correspond to _IOR(60, 0, char[20]) and _IOR(60, 1, __u16).
IOC_READ = 2
IOC_TYPE = 60
COOKED_IOCTL = (IOC_READ << 30) | (20 << 16) | (IOC_TYPE << 8) | 0
RAW_IOCTL = (IOC_READ << 30) | (2 << 16) | (IOC_TYPE << 8) | 1

kind = os.path.basename(device).rsplit("-", 1)[-1]

def raw_to_cooked(raw):
    # These formulas mirror mk-lunix-lookup.c; values are in thousandths.
    if kind in ("temp", "temperature"):
        if raw == 0:
            milli = -272150
        else:
            rth = (10000.0 * (1023.0 - raw)) / raw
            if rth <= 0:
                milli = -272150
            else:
                log_rth = math.log(rth)
                kelvin_inv = 0.001010024 + 0.000242127 * log_rth + 0.000000146 * log_rth**3
                milli = int(((1.0 / kelvin_inv) - 272.15) * 1000)
                milli = max(milli, -272150)
    elif kind in ("batt", "battery"):
        milli = int((1.223 * (1023.0 / raw) if raw else 0) * 1000)
    elif kind in ("light",):
        milli = int(raw * 5000000.0 / 65535)
    else:
        raise SystemExit(f"Cannot infer measurement type from {device!r}; use a *-temp, *-batt, or *-light node.")

    sign = "-" if milli < 0 else ""
    whole, fraction = divmod(abs(milli), 1000)
    return f"{sign}{whole}.{fraction:03d}"

fd = os.open(device, os.O_RDONLY)
try:
    print(f"Monitoring {device}; press Ctrl+C to stop.", flush=True)
    while True:
        cooked_buf = bytearray(20)
        fcntl.ioctl(fd, COOKED_IOCTL, cooked_buf, True)
        cooked = cooked_buf.split(b"\0", 1)[0].decode("ascii").strip()

        raw_buf = bytearray(2)
        fcntl.ioctl(fd, RAW_IOCTL, raw_buf, True)
        raw = struct.unpack("=H", raw_buf)[0]
        converted = raw_to_cooked(raw)
        now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        print(f"[{now}] cooked ioctl: {cooked}")
        print(f"[{now}] raw ioctl:    {raw}")
        print(f"[{now}] raw -> cooked: {converted}")
        print()
finally:
    os.close(fd)
PY
