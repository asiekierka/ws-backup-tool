/**
 * Copyright (c) 2022, 2023 Adrian Siekierka
 *
 * BootFriend is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * BootFriend is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with BootFriend. If not, see <https://www.gnu.org/licenses/>. 
 */

#ifndef __UI_H__
#define __UI_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SCREEN1 ((uint16_t*) 0x1800)

#define COLOR_BLACK 0
#define COLOR_GRAY 1
#define COLOR_RED 4
#define COLOR_YELLOW 5
#define COLOR_WHITE 6
#define COLOR_SELECTED 10

void ui_init(void);
void ui_clear_lines(uint8_t y_from, uint8_t y_to);
void ui_puts(uint8_t x, uint8_t y, uint8_t color, const char __far* buf);
static inline void ui_puts_centered(uint8_t y, uint8_t color, const char __far* buf) {
    ui_puts((28 - strlen(buf)) >> 1, y, color, buf);
}
void ui_printf(uint8_t x, uint8_t y, uint8_t color, const char __far* format, ...);

#define MENU_ENTRY_DISABLED 0x0001
#define MENU_ENTRY_ADJUSTABLE 0x0002
#define MENU_ENTRY_ADJUSTABLE_ADV 0x0004

#define RESULT_A 0x0000
#define RESULT_ADJUST_LEFT 0x0100
#define RESULT_ADJUST_RIGHT 0x0200
#define RESULT_ADJUST_COARSE_LEFT 0x0300
#define RESULT_ADJUST_COARSE_RIGHT 0x0400
#define RESULT_ADJUST_FINE_LEFT 0x0500
#define RESULT_ADJUST_FINE_RIGHT 0x0600

typedef struct {
    const char __far *text;
    uint16_t flags;
} menu_entry_t;

typedef struct {
    menu_entry_t *entries;
    uint8_t entry_count;
    uint8_t curr_entry;
} menu_state_t;

void ui_menu_init(menu_state_t *state);
uint16_t ui_menu_run(menu_state_t *state, uint8_t y);

#endif /* __UI_H__ */
