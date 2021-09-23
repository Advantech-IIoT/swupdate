/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _RECOVERY_UI_H
#define _RECOVERY_UI_H

#include <stdio.h>

// Initialize the graphics system.
void ui_init();

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h".
int ui_wait_key();            // waits for a key/button press, returns the code
int ui_key_pressed(int key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
void ui_show_text(int visible);
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Display some header text followed by a menu of items, which appears
// at the top of the screen (in place of any scrolling ui_print()
// output, if necessary).
void ui_start_menu(char** headers, char** items, int initial_selection);
// Set the menu highlight to the given index, and return it (capped to
// the range [0..numitems).
int ui_menu_select(int sel);
// End menu mode, resetting the text overlay so that ui_print()
// statements will be displayed.
void ui_end_menu();

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_ICON_INSTALLING,
  BACKGROUND_ICON_ERROR,
  NUM_BACKGROUND_ICONS
};
void ui_set_background(int icon);

// Show a progress bar and define the scope of the next operation:
//   portion - fraction of the progress bar the next operation will use
//   seconds - expected time interval (progress bar moves at this minimum rate)
void ui_show_progress(float portion, int seconds);
void ui_show_progress2(int progress);
void ui_set_progress(float fraction);  // 0.0 - 1.0 within the defined scope

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

// Show a rotating "barberpole" for ongoing operations.  Updates automatically.
void ui_show_indeterminate_progress();

// Hide and reset the progress bar.
void ui_reset_progress();

#define LOGE(...) fprintf(stdout, "E:" __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stdout, "I:" __VA_ARGS__)

#if 0
#define LOGV(...) fprintf(stdout, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

typedef struct {
    const char* mount_point;  // eg. "/cache".  must live in the root directory.

    const char* fs_type;      // "yaffs2" or "ext4" or "vfat"

    const char* device;       // MTD partition name if fs_type == "yaffs"
                              // block device if fs_type == "ext4" or "vfat"

    const char* device2;      // alternative device to try if fs_type
                              // == "ext4" or "vfat" and mounting
                              // 'device' fails

    const char* option;       // mount parameter

    const char* dump;         //

    const char* pass;         //
} Volume;

// Called when recovery starts up.  Returns 0.
extern int device_recovery_start();

// Called in the input thread when a new key (key_code) is pressed.
// *key_pressed is an array of KEY_MAX+1 bytes indicating which other
// keys are already pressed.  Return true if the text display should
// be toggled.
extern int device_toggle_display(volatile char* key_pressed, int key_code);

// Called in the input thread when a new key (key_code) is pressed.
// *key_pressed is an array of KEY_MAX+1 bytes indicating which other
// keys are already pressed.  Return true if the device should reboot
// immediately.
extern int device_reboot_now(volatile char* key_pressed, int key_code);

// Called from the main thread when recovery is waiting for input and
// a key is pressed.  key is the code of the key pressed; visible is
// true if the recovery menu is being shown.  Implementations can call
// ui_key_pressed() to discover if other keys are being held down.
// Return one of the defined constants below in order to:
//
//   - move the menu highlight (HIGHLIGHT_*)
//   - invoke the highlighted item (SELECT_ITEM)
//   - do nothing (NO_ACTION)
//   - invoke a specific action (a menu position: any non-negative number)
extern int device_handle_key(int key, int visible);

// Perform a recovery action selected from the menu.  'which' will be
// the item number of the selected menu item, or a non-negative number
// returned from device_handle_key().  The menu will be hidden when
// this is called; implementations can call ui_print() to print
// information to the screen.
extern int device_perform_action(int which);

// Called when we do a wipe data/factory reset operation (either via a
// reboot from the main system with the --wipe_data flag, or when the
// user boots into recovery manually and selects the option from the
// menu.)  Can perform whatever device-specific wiping actions are
// needed.  Return 0 on success.  The userdata and cache partitions
// are erased after this returns (whether it returns success or not).
int device_wipe_data();

#define NO_ACTION           -1

#define HIGHLIGHT_UP        -2
#define HIGHLIGHT_DOWN      -3
#define SELECT_ITEM         -4

#define ITEM_REBOOT          0
#define ITEM_APPLY_SDCARD    1
#define ITEM_WIPE_DATA       2
#define ITEM_WIPE_CACHE      3

// Header text to display above the main menu.
extern char* MENU_HEADERS[];

// Text of menu items.
extern char* MENU_ITEMS[];

#endif
