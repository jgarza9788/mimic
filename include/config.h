#ifndef MIMIC_CONFIG_H
#define MIMIC_CONFIG_H

#include <xcb/xcb.h>

#define WM_NAME "mimicwm"

/* Core behavior */
#define WORKSPACE_COUNT 4
#define BORDER_WIDTH 2
#define MOVE_STEP 25
#define RESIZE_STEP 30

/* Modifiers */
#define MOD_MASK XCB_MOD_MASK_4

/* Colors */
#define COLOR_BORDER_FOCUS 0x66aaff
#define COLOR_BORDER_NORMAL 0x333333

/* Commands */
#define TERMINAL_CMD "xterm"
#define MENU_CMD "dmenu_run"

#endif
