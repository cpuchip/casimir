#!/usr/bin/env bash
# Builds, flashes and runs casim's bench on an ESP32-S3 (default: casim's XIAO on lepton).
#
#   scripts/device.sh [/dev/ttyACM2]
#
# The report is printed and kept in build/device-s3.log; exit 0 on DEVICE PASS (every derivative on
# the chip equals the host's). The toolchain is the pinned image's; the port goes into the container
# with its group, so the host user needs no serial access of its own.
set -euo pipefail
PORT="${1:-/dev/ttyACM2}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -s build/casim
python3 scripts/device_cases.py device/main/generated/cases.c
docker run --rm -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$PORT")" --device "$PORT" \
    -e HOME=/tmp -e PORT="$PORT" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
TARGET=esp32s3 build/core/scripts/fetch-intel-dfp.sh
cd device
B=/project/build/device
# sdkconfig is regenerated from sdkconfig.defaults whenever the defaults are newer
[ -f "$B/sdkconfig" ] && [ sdkconfig.defaults -nt "$B/sdkconfig" ] && rm -f "$B/sdkconfig"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 size-components | grep -E "libmain|Total" || true
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 -p "$PORT" flash
python /project/scripts/device_capture.py "$PORT" /project/build/device-s3.log 600
'
