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

#define FLASH_MODE_SLOW 0x00
#define FLASH_MODE_FAST_WONDERWITCH 0x01
#define FLASH_MODE_FAST_FLASHMASTA 0x02
#define FLASH_MODE_FAST_MX29L 0x03

bool flash_write(const void *data, uint16_t offset, uint16_t len, uint16_t mode);
bool flash_erase(uint16_t offset, uint16_t mode);
