#include <stdio.h>

#include "wm.h"

int main(void) {
    /* Lifecycle: initialize resources, run the event loop, then clean up. */
    wm_init();
    wm_run();
    wm_cleanup();
    return 0;
}
