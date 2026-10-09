/* Forced into Intel's sources in the ESP-IDF build (CMakeLists.txt) in place of a bare fenv.h. The library calls
 * fegetround and fesetround only to put its binary floating point in round-to-nearest for a moment and restore the
 * old mode after (bid128_div.c, bid128_sqrt.c and their kin). On the ESP32's cores, binary double is the compiler's
 * software floating point, which always rounds to nearest, so the calls are compiled out; newlib's xtensa libm also
 * defines fesetround in two members, which fails the link when both are pulled in. The calculator firmware builds
 * the library the same way (its scripts/fenv-nearest.h); the S3 bench, byte-identical to the host, is the check. */
#include <fenv.h>
#define fegetround() FE_TONEAREST
#define fesetround(mode) ((mode) == FE_TONEAREST ? 0 : 1)
