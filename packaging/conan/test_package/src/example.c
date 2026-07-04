/* Minimal consumer of the rednoise C ABI. Links rednoise::rednoise and calls
 * into the installed library to prove the package is usable from pure C. */
#include <rednoise/rednoise.h>
#include <stdio.h>

int main(void) {
    printf("rednoise version: %s\n", rn_version());
    return 0;
}
