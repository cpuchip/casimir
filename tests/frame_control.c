/* The frame guard's planted control (CAS 004 rule 3): a 2 KB frame, which -Wframe-larger-than=1024 must
 * refuse. The Makefile and scripts/size-s3.sh compile it expecting failure; if it ever compiles, the
 * guard is checking nothing. Not a test binary and never linked. */
#include <string.h>

int frame_control(const char *s);

int frame_control(const char *s)
{
    volatile char big[2048];
    memset((char *)big, 0, sizeof big);
    big[strlen(s) % sizeof big] = 1;
    return big[0] + big[1];
}
