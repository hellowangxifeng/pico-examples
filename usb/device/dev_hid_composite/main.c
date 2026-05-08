/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

static uint8_t keycode_from_name(const char* key)
{
    if (strlen(key) == 1) {
        char c = key[0];

        if (c >= 'a' && c <= 'z') {
            return HID_KEY_A + (c - 'a');
        }

        if (c >= 'A' && c <= 'Z') {
            return HID_KEY_A + (c - 'A');
        }

        if (c >= '1' && c <= '9') {
            return HID_KEY_1 + (c - '1');
        }

        if (c == '0') {
            return HID_KEY_0;
        }
    }

    if (!strcmp(key, "ENTER")) return HID_KEY_ENTER;
    if (!strcmp(key, "ESC")) return HID_KEY_ESCAPE;
    if (!strcmp(key, "SPACE")) return HID_KEY_SPACE;
    if (!strcmp(key, "TAB")) return HID_KEY_TAB;
    if (!strcmp(key, "BACKSPACE")) return HID_KEY_BACKSPACE;
    if (!strcmp(key, "UP")) return HID_KEY_ARROW_UP;
    if (!strcmp(key, "DOWN")) return HID_KEY_ARROW_DOWN;
    if (!strcmp(key, "LEFT")) return HID_KEY_ARROW_LEFT;
    if (!strcmp(key, "RIGHT")) return HID_KEY_ARROW_RIGHT;

    return 0;
}

static void send_key(uint8_t key)
{
    if (!tud_hid_ready() || key == 0) return;

    uint8_t keycode[6] = {0};
    keycode[0] = key;

    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    sleep_ms(20);
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
}

static void send_mouse_move(int dx, int dy)
{
    if (!tud_hid_ready()) return;

    while (dx != 0 || dy != 0) {
        int8_t step_x = dx > 127 ? 127 : dx < -127 ? -127 : dx;
        int8_t step_y = dy > 127 ? 127 : dy < -127 ? -127 : dy;

        tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, step_x, step_y, 0, 0);

        dx -= step_x;
        dy -= step_y;

        sleep_ms(5);
    }
}

static void send_click(uint8_t button)
{
    if (!tud_hid_ready()) return;

    tud_hid_mouse_report(REPORT_ID_MOUSE, button, 0, 0, 0, 0);
    sleep_ms(30);
    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, 0, 0, 0, 0);
}

static void send_scroll(int amount)
{
    if (!tud_hid_ready()) return;

    while (amount != 0) {
        int8_t step = amount > 127 ? 127 : amount < -127 ? -127 : amount;

        tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, 0, 0, step, 0);

        amount -= step;
        sleep_ms(5);
    }
}

static void type_text(const char* text)
{
    while (*text) {
        char c = *text++;

        uint8_t modifier = 0;
        uint8_t key = 0;

        if (c >= 'a' && c <= 'z') {
            key = HID_KEY_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            key = HID_KEY_A + (c - 'A');
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        } else if (c >= '1' && c <= '9') {
            key = HID_KEY_1 + (c - '1');
        } else if (c == '0') {
            key = HID_KEY_0;
        } else if (c == ' ') {
            key = HID_KEY_SPACE;
        } else if (c == '\n') {
            key = HID_KEY_ENTER;
        }

        if (key && tud_hid_ready()) {
            uint8_t keycode[6] = {0};
            keycode[0] = key;

            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycode);
            sleep_ms(20);
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
            sleep_ms(5);
        }
    }
}

static void handle_command(char* line)
{
    char* cmd = strtok(line, " \r\n");
    if (!cmd) return;

    if (!strcmp(cmd, "MOVE")) {
        char* sx = strtok(NULL, " \r\n");
        char* sy = strtok(NULL, " \r\n");
        if (sx && sy) {
            send_mouse_move(atoi(sx), atoi(sy));
        }
    }
    else if (!strcmp(cmd, "CLICK")) {
        char* btn = strtok(NULL, " \r\n");

        if (!btn || !strcmp(btn, "LEFT")) {
            send_click(MOUSE_BUTTON_LEFT);
        } else if (!strcmp(btn, "RIGHT")) {
            send_click(MOUSE_BUTTON_RIGHT);
        } else if (!strcmp(btn, "MIDDLE")) {
            send_click(MOUSE_BUTTON_MIDDLE);
        }
    }
    else if (!strcmp(cmd, "KEY")) {
        char* keyname = strtok(NULL, " \r\n");
        if (keyname) {
            send_key(keycode_from_name(keyname));
        }
    }
    else if (!strcmp(cmd, "SCROLL")) {
        char* amount = strtok(NULL, " \r\n");
        if (amount) {
            send_scroll(atoi(amount));
        }
    }
    else if (!strcmp(cmd, "TEXT")) {
        char* text = strtok(NULL, "\r\n");
        if (text) {
            type_text(text);
        }
    }
}

int main(void)
{
    board_init();

    // init TinyUSB device stack
    tud_init(BOARD_TUD_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    char line[128];
    int pos = 0;

    while (1) {
        tud_task();

        while (tud_cdc_available()) {
            char c;
            tud_cdc_read(&c, 1);

            if (c == '\n' || c == '\r') {
                line[pos] = '\0';

                if (pos > 0) {
                    handle_command(line);
                }

                pos = 0;
            } else {
                if (pos < sizeof(line) - 1) {
                    line[pos++] = c;
                }
            }
        }
    }

    return 0;
}
}
