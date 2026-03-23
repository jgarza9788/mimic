#include <stdio.h>

#include "wm.h"

// Handles main for mimicwm.
// Keeps behavior localized to this function for easier maintenance.
int main(void) {
    wm_init();
    wm_run();
    wm_cleanup();
    return 0;
}
