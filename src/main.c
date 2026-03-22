#include <stdio.h>

#include "wm.h"

int main(void) {
    wm_init();
    wm_run();
    wm_cleanup();
    return 0;
}
