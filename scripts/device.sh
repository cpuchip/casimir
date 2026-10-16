#!/usr/bin/env bash
# Builds, flashes and runs casim's bench on an ESP32-S3, through Casimir's ESP-IDF component.
#
#   PORT=/dev/ttyACM0 scripts/device.sh    (or the port as the first argument)
#   scripts/device.sh --build               build only, no chip (CI: the ESP-IDF component builds)
#   CASIM_BENCH_ARENA=12288 scripts/device.sh ...   host and chip both at that arena (default: the host 65,536,
#                                           the chip 32,768; scripts/device_cases.py)
#
# The report is printed and kept in build/device-s3.log; exit 0 on DEVICE PASS (every derivative on
# the chip equals the host's). The toolchain is the pinned image's; the port goes into the container
# with its group, so the host user needs no serial access of its own.
set -euo pipefail
BUILD_ONLY=0
if [ "${1:-}" = "--build" ]; then BUILD_ONLY=1; PORT=""; else
    PORT="${1:-${PORT:?the S3s serial port: PORT=/dev/ttyACM0 scripts/device.sh}}"; fi
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -s build/casim build/intel/libbid.a
python3 scripts/device_cases.py device/main/generated/cases.c
DEV=()
[ "$BUILD_ONLY" = 1 ] || DEV=(--group-add "$(stat -c %g "$PORT")" --device "$PORT")
docker run --rm --init --name "casim-device-$$" --memory 8g -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" "${DEV[@]}" \
    -e HOME=/tmp -e PORT="$PORT" -e BUILD_ONLY="$BUILD_ONLY" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
cd device
T=-DCASIM_INTEL_TARBALL=/project/build/intel-dfp/IntelRDFPMathLib20U5.tar.gz
B=/project/build/device
# sdkconfig is regenerated from sdkconfig.defaults whenever the defaults are newer
[ -f "$B/sdkconfig" ] && [ sdkconfig.defaults -nt "$B/sdkconfig" ] && rm -f "$B/sdkconfig"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 $T build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 size-components | grep -E "libmain|libcasim|Total" || true
[ "$BUILD_ONLY" = 1 ] && exit 0
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 -p "$PORT" flash
# the first whole report comes after every case has run: 608 s at 897 cases (CAS 014)
python /project/scripts/device_capture.py "$PORT" /project/build/device-s3.log 1500
'
