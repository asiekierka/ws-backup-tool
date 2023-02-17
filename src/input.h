/**
 * Copyright (c) 2022, 2023 Adrian Siekierka
 *
 * WS Backup Tool is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * WS Backup Tool is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with WS Backup Tool. If not, see <https://www.gnu.org/licenses/>. 
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <ws.h>

extern uint16_t input_keys, input_pressed, input_held;

#define KEY_UP KEY_X1
#define KEY_DOWN KEY_X3
#define KEY_LEFT KEY_X4
#define KEY_RIGHT KEY_X2

#define KEY_AUP KEY_Y1
#define KEY_ADOWN KEY_Y3
#define KEY_ALEFT KEY_Y4
#define KEY_ARIGHT KEY_Y2

void vblank_input_update(void);
void input_reset(void);
void input_update(void);
void input_wait_clear(void);
