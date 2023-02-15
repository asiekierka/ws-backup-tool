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

#include <stdbool.h>
#include <stdint.h>
#include <wonderful.h>
#include <ws.h>
#include "flash.h"
#include "font_default.h"
#include "input.h"
#include "nanoprintf.h"
#include "ui.h"
#include "util.h"
#include "wonderful-asm-common.h"
#include "ws/display.h"
#include "ws/eeprom.h"
#include "ws/hardware.h"
#include "xmodem.h"

volatile uint16_t vbl_ticks;
volatile uint8_t xm_baudrate;

__attribute__((interrupt))
void vblank_int_handler(void) {
	vbl_ticks++;
	vblank_input_update();
	ws_hwint_ack(HWINT_VBLANK);
}

static const char __far msg_are_you_sure[] = "Are you sure?";
static const char __far msg_yes[] = "Yes";
static const char __far msg_no[] = "No";

static inline void xmodem_open_default() {
	xmodem_open(xm_baudrate);
}

static const char __far msg_xmodem_init[] = "Initializing XMODEM transfer";
static const char __far msg_xmodem_progress[] = "Transferring data";
static const char __far msg_erase_progress[] = "Erasing data";
static const char __far msg_xmodem_transfer_error[] = "Transfer error";
static const char __far msg_xmodem_blocks_full[] = "0000/%04d";

static void xmodem_status(const char __far *str) {
	ui_clear_lines(6, 6);
	ui_puts_centered(6, COLOR_BLACK, str);
}

typedef const uint8_t __far* (*xmodem_block_reader)(uint16_t block, uint16_t subblock);

static void xmodem_update_counter(uint8_t x, uint8_t y, uint16_t value) {
	ws_screen_put(SCREEN1, (value % 10) + ((uint8_t)'0' | SCR_ENTRY_PALETTE(COLOR_WHITE)), x + 3, y); value /= 10; if (value == 0) return;
	ws_screen_put(SCREEN1, (value % 10) + ((uint8_t)'0' | SCR_ENTRY_PALETTE(COLOR_WHITE)), x + 2, y); value /= 10; if (value == 0) return;
	ws_screen_put(SCREEN1, (value % 10) + ((uint8_t)'0' | SCR_ENTRY_PALETTE(COLOR_WHITE)), x + 1, y); value /= 10; if (value == 0) return;
	ws_screen_put(SCREEN1, (value % 10) + ((uint8_t)'0' | SCR_ENTRY_PALETTE(COLOR_WHITE)), x,     y);
}

void xmodem_run_send(xmodem_block_reader reader, uint16_t blocks, uint16_t subblocks) {
	uint16_t block_mask = (blocks >> 4); if(block_mask < 1) block_mask = 1;
	uint16_t subblock_mask = (subblocks >> 4); if(subblock_mask < 1) subblock_mask = 1;

	xmodem_status(msg_xmodem_init);
	xmodem_open_default();
	if (xmodem_send_start() == XMODEM_OK) {
		xmodem_status(msg_xmodem_progress);
		ui_clear_lines(11, 11);
		ui_printf(18, 11, COLOR_WHITE, msg_xmodem_blocks_full, blocks);
		for (uint16_t ib = 0; ib < blocks; ib++) {
			if (!(ib % block_mask)) ws_screen_put(SCREEN1, SCR_ENTRY_PALETTE(COLOR_RED) | 0x0A, 1 + (ib / block_mask), 11);
			xmodem_update_counter(18, 11, ib+1);
			if(subblocks > 1) {
				ui_clear_lines(12, 12);
				ui_printf(18, 12, COLOR_WHITE, msg_xmodem_blocks_full, subblocks);
			}
			for (uint16_t isb = 0; isb < subblocks; isb++) {
				// draw block update
				if(subblocks > 1) {
					xmodem_update_counter(18, 12, isb+1);
					if (!(isb % subblock_mask)) ws_screen_put(SCREEN1, SCR_ENTRY_PALETTE(COLOR_YELLOW) | 0x0A, 1 + (isb / subblock_mask), 12);
				}

				uint8_t result = xmodem_send_block(reader(ib, isb));
				switch (result) {
				case XMODEM_OK:
					break;
				case XMODEM_ERROR:
					xmodem_status(msg_xmodem_transfer_error);
					input_wait_clear(); while (input_pressed == 0) { wait_for_vblank(); input_update(); } input_wait_clear();
				case XMODEM_SELF_CANCEL:
				case XMODEM_CANCEL:
					goto End;
				}
			}
		}
		xmodem_send_finish();
	}
End:
	xmodem_close();
	ui_clear_lines(3, 17);
}

typedef uint8_t __far* (*xmodem_block_writer)(uint16_t block, uint16_t subblock);
typedef void (*xmodem_block_writer_finish)(uint16_t block, uint16_t subblock);

void xmb_noop_write_finish(uint16_t block, uint16_t subblock) {

}

void xmodem_run_recv(xmodem_block_writer writer, xmodem_block_writer_finish wrf, uint16_t blocks, uint16_t subblocks, bool erase) {
	uint16_t block_mask = (blocks >> 4); if(block_mask < 1) block_mask = 1;
	uint16_t subblock_mask = (subblocks >> 4); if(subblock_mask < 1) subblock_mask = 1;

	if(!erase) {
		xmodem_status(msg_xmodem_init);
		xmodem_open_default();
	}
	bool ack_previous = false;

	cpu_irq_disable();
	{
		xmodem_status(erase ? msg_erase_progress : msg_xmodem_progress);
		ui_clear_lines(11, 11);
		ui_printf(18, 11, COLOR_WHITE, msg_xmodem_blocks_full, blocks);
		for (uint16_t ib = 0; ib < blocks; ib++) {
			if (!(ib % block_mask)) ws_screen_put(SCREEN1, SCR_ENTRY_PALETTE(COLOR_RED) | 0x0A, 1 + (ib / block_mask), 11);
			xmodem_update_counter(18, 11, ib+1);
			if(subblocks > 1) {
				ui_clear_lines(12, 12);
				ui_printf(18, 12, COLOR_WHITE, msg_xmodem_blocks_full, subblocks);
			}
			for (uint16_t isb = 0; isb < subblocks; isb++) {
				// draw block update
				if(subblocks > 1) {
					xmodem_update_counter(18, 12, isb+1);
					if (!(isb % subblock_mask)) ws_screen_put(SCREEN1, SCR_ENTRY_PALETTE(COLOR_YELLOW) | 0x0A, 1 + (isb / subblock_mask), 12);
				}
				if(erase) {
					memset(writer(ib, isb), 0xFF, 128);
					wrf(ib, isb);
				} else {
					uint8_t __far* block_buffer = writer(ib, isb);
					if(ack_previous) {
						xmodem_recv_ack();
					} else {
						xmodem_recv_start();
					}
					uint8_t result = xmodem_recv_block(block_buffer);
					switch (result) {					
					case XMODEM_OK:
						wrf(ib, isb);
						if(!erase) ack_previous = true;
						break;
					case XMODEM_ERROR:
						xmodem_status(msg_xmodem_transfer_error);
						input_wait_clear(); while (input_pressed == 0) { wait_for_vblank(); input_update(); } input_wait_clear();
					case XMODEM_SELF_CANCEL:
					case XMODEM_CANCEL:
						goto End;
					case XMODEM_COMPLETE:
						goto End;
					}
				}
			}
		}
	}
End:
	ws_hwint_ack(0xFF);
	cpu_irq_enable();
	if(!erase) xmodem_close();
	ui_clear_lines(3, 17);
}

static bool menu_manip_value(uint32_t *value, uint32_t command,
	int32_t min_value, int32_t max_value,
	int32_t prev_value, int32_t next_value,
	int32_t prev_value_fine, int32_t next_value_fine,
	int32_t prev_value_coarse, int32_t next_value_coarse
) {
	int32_t new_value = *value;
	switch (command & 0xFF00) {
	case RESULT_A:
		if (next_value == new_value || next_value > max_value) {
			new_value = min_value;
		} else {
			new_value = next_value;
		}
		break;
	case RESULT_ADJUST_LEFT: new_value = prev_value; break;
	case RESULT_ADJUST_RIGHT: new_value = next_value; break;
	case RESULT_ADJUST_COARSE_LEFT: new_value = prev_value_coarse; break;
	case RESULT_ADJUST_COARSE_RIGHT: new_value = next_value_coarse; break;
	case RESULT_ADJUST_FINE_LEFT: new_value = prev_value_fine; break;
	case RESULT_ADJUST_FINE_RIGHT: new_value = next_value_fine; break;
	default: return false;
	}
	if (new_value < min_value) new_value = min_value;
	else if (new_value > max_value) new_value = max_value;
	*value = new_value;
	return true;
}

bool menu_confirm(const char __far *text, uint8_t text_height, bool centered) {
	menu_state_t state;
	menu_entry_t entries[2];
	uint8_t height = text_height + 3;
	uint8_t y_text = 3 + ((14 - height) >> 1);
	uint8_t y_menu = y_text + text_height + 1;

	ui_puts(centered ? (28 - strlen(text)) >> 1 : 0, y_text, 0, text);

	entries[0].text = msg_no;  entries[0].flags = 0;
	entries[1].text = msg_yes; entries[1].flags = 0;
	state.entries = entries; state.entry_count = 2;
	ui_menu_init(&state);
	uint8_t result = ui_menu_run(&state, y_menu);

	ui_clear_lines(y_text, y_text + text_height - 1);
	return result == 1;
}

static const char __far msg_send_ipl[] = "Transfer IPL...";
static const char __far msg_backup[] = "Cart Backup \x10";
static const char __far msg_restore[] = "Cart Restore \x10";
static const char __far msg_erase[] = "Cart Erase \x10";
static const char __far msg_flash[] = "Cart Flash (Expert) \x10";
static const char __far msg_baud_38400[] = "Serial: 38400 bps";
static const char __far msg_baud_9600[] = "Serial: .9600 bps";

static const char __far msg_none[] = "";
static const char __far msg_rom_full[] = "ROM: %ld Mbit";
static const char __far msg_rom_half[] = "ROM: %ld.5 Mbit";
static const char __far msg_sram[] = "SRAM: %ld Kbyte";
static const char __far msg_eeprom[] = "EEPROM: %ld bytes";

static const char __far msg_backup_rom[] = "Backup ROM...";
static const char __far msg_backup_sram[] = "Backup SRAM...";
static const char __far msg_backup_eeprom[] = "Backup EEPROM...";

static const char __far msg_restore_sram[] = "Restore SRAM...";
static const char __far msg_restore_eeprom[] = "Restore EEPROM...";

static const char __far msg_erase_sram[] = "Erase SRAM...";
static const char __far msg_erase_eeprom[] = "Erase EEPROM...";

static const char __far msg_return[] = "\x1b Return";

uint16_t xmb_offset;
uint8_t xmb_mode;
uint8_t xmb_buffer[128];

// block: 128 bytes
const uint8_t __far* xmb_ipl_read(uint16_t block, uint16_t subblock) {
	return MK_FP(0xFE00, block << 7);
}

// block: bank; subblock: 128 bytes
const uint8_t __far* xmb_rom_read(uint16_t block, uint16_t subblock) {
	if (subblock == 0) {
		uint16_t bank = xmb_offset + block;
		outportw(IO_BANK_2003_ROM0, bank);
		outportb(IO_BANK_ROM0, bank);
	}
	return MK_FP(0x2000, subblock << 7);
}

// block: 8kbytes; subblock: 128 bytes
uint8_t __far* xmb_sram_read(uint16_t block, uint16_t subblock) {
	uint16_t bank = block >> 3;
	uint16_t kbyte = block & 0x07;
	if (kbyte == 0) {
		bank += xmb_offset;
		outportw(IO_BANK_2003_RAM, bank);
		outportb(IO_BANK_RAM, bank);
	}
	return MK_FP(0x1000 | (kbyte << 9), subblock << 7);
}

// block: eeprom 128b
const uint8_t __far* xmb_eeprom_read(uint16_t block, uint16_t subblock) {
	ws_eeprom_handle_t h = ws_eeprom_handle_cartridge(xmb_offset);
	uint16_t *ptr = (uint16_t*) xmb_buffer;
	uint16_t p = (block << 7);
	for (uint16_t i = 0; i < 128; i += 2, p += 2) {
		*(ptr++) = ws_eeprom_read_word(h, p);
	}
	return xmb_buffer;
}

uint8_t __far* xmb_eeprom_write(uint16_t block, uint16_t subblock) {
	return xmb_buffer;
}

void xmb_eeprom_write_finish(uint16_t block, uint16_t subblock) {
	ws_eeprom_handle_t h = ws_eeprom_handle_cartridge(xmb_offset);
	ws_eeprom_write_unlock(h);
	uint16_t *ptr = (uint16_t*) xmb_buffer;
	uint16_t p = (block << 7);
	for (uint16_t i = 0; i < 128; i += 2, p += 2) {
		ws_eeprom_write_word(h, p, *(ptr++));
	}
	ws_eeprom_write_lock(h);
}

void menu_backup(bool restore, bool erase) {
	char buf_rom[20], buf_sram[20], buf_eeprom[20];
	menu_state_t state;
	menu_entry_t entries[9];
	uint8_t entry_count = 0;

	uint32_t rom_banks = 256;
	uint32_t sram_kbytes = 0;
	uint32_t eeprom_bytes = 0;
	
	// generate menu entry list
	if (!restore) {
		entries[entry_count].text = buf_rom;
		entries[entry_count++].flags = MENU_ENTRY_ADJUSTABLE | MENU_ENTRY_ADJUSTABLE_ADV;
	}
	entries[entry_count].text = buf_sram;
	entries[entry_count++].flags = MENU_ENTRY_ADJUSTABLE | MENU_ENTRY_ADJUSTABLE_ADV;
	entries[entry_count].text = buf_eeprom;
	entries[entry_count++].flags = MENU_ENTRY_ADJUSTABLE;
	entries[entry_count].text = msg_none;
	entries[entry_count++].flags = MENU_ENTRY_DISABLED;
	if (!restore) {
		entries[entry_count].text = msg_backup_rom;
		entries[entry_count++].flags = 0;
		entries[entry_count].text = msg_backup_sram;
		entries[entry_count++].flags = 0;
		entries[entry_count].text = msg_backup_eeprom;
		entries[entry_count++].flags = 0;
	} else {
		entries[entry_count].text = erase ? msg_erase_sram : msg_restore_sram;
		entries[entry_count++].flags = 0;
		entries[entry_count].text = erase ? msg_erase_eeprom : msg_restore_eeprom;
		entries[entry_count++].flags = 0;
	}
	entries[entry_count].text = msg_return;
	entries[entry_count++].flags = 0;
	state.entries = entries; state.entry_count = entry_count;	
	ui_menu_init(&state);

	// determine banks
	outportw(IO_BANK_2003_ROM0, 0xFFFF);
	outportb(IO_BANK_ROM0, 0xFF);
	switch (*((uint8_t __far*) MK_FP(0x2FFF, 0xA))) {
	case 0: rom_banks = 2; break;
	case 1: rom_banks = 4; break;
	case 2: rom_banks = 8; break;
	case 3: rom_banks = 16; break;
	case 4: rom_banks = 32; break;
	case 5: rom_banks = 48; break;
	case 6: rom_banks = 64; break;
	case 7: rom_banks = 96; break;
	case 8: rom_banks = 128; break;
	case 9: rom_banks = 256; break;
	case 10: rom_banks = 512; break;
	case 11: rom_banks = 1024; break;
	}
	switch (*((uint8_t __far*) MK_FP(0x2FFF, 0xB))) {
	case 0x01: sram_kbytes = 8; break;
	case 0x02: sram_kbytes = 32; break;
	case 0x03: sram_kbytes = 128; break;
	case 0x04: sram_kbytes = 256; break;
	case 0x05: sram_kbytes = 512; break;
	case 0x10: eeprom_bytes = 128; break;
	case 0x20: eeprom_bytes = 2048; break;
	case 0x50: eeprom_bytes = 1024; break;
	}

	while (true) {
		// update ROM/SRAM/EEPROM strings
		if (!restore) {
			npf_snprintf(buf_rom, sizeof(buf_rom), (rom_banks & 1) ? msg_rom_half : msg_rom_full, rom_banks >> 1);
		}
		npf_snprintf(buf_sram, sizeof(buf_sram), msg_sram, sram_kbytes);
		npf_snprintf(buf_eeprom, sizeof(buf_eeprom), msg_eeprom, eeprom_bytes);

		uint16_t result = ui_menu_run(&state, 3 + ((14 - entry_count) >> 1));
		if (restore) {
			result++;
			if ((result & 0xFF) > 3) result++;
		}
		switch (result & 0xFF) {
		case 0:
			menu_manip_value(&rom_banks, result, 1, 1024,
				rom_banks >> 1, rom_banks << 1,
				rom_banks - 1, rom_banks + 1,
				rom_banks - 16, rom_banks + 16);
			break;
		case 1:
			menu_manip_value(&sram_kbytes, result, 8, 65536,
				sram_kbytes >> 1, sram_kbytes << 1,
				sram_kbytes - 8, sram_kbytes + 8,
				sram_kbytes - 64, sram_kbytes + 64);
			break;
		case 2:
			menu_manip_value(&eeprom_bytes, result, 128, 2048,
				eeprom_bytes >> 1, eeprom_bytes << 1,
				0, 0, 0, 0);
			break;
		case 4:
			xmb_offset = (rom_banks ^ 0xFFFF) + 1;
			if (!restore) {
				xmodem_run_send(xmb_rom_read, rom_banks, 512);
			}
			break;
		case 5:
			xmb_offset = ((sram_kbytes >> 6) ^ 0xFFFF) + 1;
			if (!restore) {
				xmodem_run_send(xmb_sram_read, sram_kbytes >> 3, 64);
			} else {
				xmodem_run_recv(xmb_sram_read, xmb_noop_write_finish, sram_kbytes >> 3, 64, erase);
			}
			break;
		case 6:
			xmb_offset = eeprom_bytes <= 128 ? 6 : (eeprom_bytes <= 512 ? 8 : 10);
			if (!restore) {
				xmodem_run_send(xmb_eeprom_read, eeprom_bytes >> 7, 1);
			} else {
				xmodem_run_recv(xmb_eeprom_write, xmb_eeprom_write_finish, eeprom_bytes >> 7, 1, erase);
			}
			break;
		case 7:
			return;
		}
	}
}

static const char __far msg_offset_from_end[] = "End Offset: %ld KB";
static const char __far msg_kbytes[] = "Size: %ld KB";
static const char __far msg_write_flash[] = "Write Flash...";
static const char __far msg_flash_mode_regular[] = "Mode: Regular";
static const char __far msg_flash_mode_wonderwitch[] = "Mode: WonderWitch";
static const char __far msg_flash_mode_flashmasta[] = "Mode: WSFM";

// return offset
static uint16_t xmf_acquire_kbyte(uint16_t kbyte) {
	uint16_t bank = 0xFC00 | ((xmb_offset + kbyte) >> 6);
	outportw(IO_BANK_2003_RAM, bank);
	outportb(IO_BANK_RAM, bank);
	return (kbyte << 10);
}

uint8_t __far* xmf_write(uint16_t block, uint16_t subblock) {
	return xmb_buffer;
}

void xmf_erase_finish(uint16_t block, uint16_t subblock) {
	if (subblock == 0) {
		uint16_t offset = xmf_acquire_kbyte(block);
		flash_erase(offset, xmb_mode);
	}
}

void xmf_write_finish(uint16_t block, uint16_t subblock) {
	uint16_t offset = xmf_acquire_kbyte(block);
	flash_write(xmb_buffer, offset + (subblock << 7), 128, xmb_mode);
}

void menu_flash(void) {
	char buf_offset_from_end[30], buf_kbytes[30];
	menu_state_t state;
	menu_entry_t entries[6];
	uint8_t entry_count = 0;

	uint32_t offset_from_end = 0;
	uint32_t kbytes = 64;
	uint8_t mode = 0;
	
	// generate menu entry list
	entries[entry_count].text = buf_offset_from_end;
	entries[entry_count++].flags = MENU_ENTRY_ADJUSTABLE | MENU_ENTRY_ADJUSTABLE_ADV;
	entries[entry_count].text = buf_kbytes;
	entries[entry_count++].flags = MENU_ENTRY_ADJUSTABLE | MENU_ENTRY_ADJUSTABLE_ADV;
	entries[entry_count].text = msg_flash_mode_regular;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_none;
	entries[entry_count++].flags = MENU_ENTRY_DISABLED;
	entries[entry_count].text = msg_write_flash;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_return;
	entries[entry_count++].flags = 0;
	state.entries = entries; state.entry_count = entry_count;	
	ui_menu_init(&state);
	while (true) {
		npf_snprintf(buf_offset_from_end, sizeof(buf_offset_from_end), msg_offset_from_end, offset_from_end);
		npf_snprintf(buf_kbytes, sizeof(buf_kbytes), msg_kbytes, kbytes);
		switch (mode) {
			case 0: entries[2].text = msg_flash_mode_regular; break;
			case 1: entries[2].text = msg_flash_mode_wonderwitch; break;
			case 2: entries[2].text = msg_flash_mode_flashmasta; break;
		}

		uint16_t result = ui_menu_run(&state, 3 + ((14 - entry_count) >> 1));
		switch (result & 0xFF) {
		case 0:
			menu_manip_value(&offset_from_end, result, 0, 65535,
				offset_from_end - 64, offset_from_end + 64,
				offset_from_end - 1, offset_from_end + 1,
				offset_from_end - 1024, offset_from_end + 1024);
			break;
		case 1:
			menu_manip_value(&kbytes, result, 1, 8192,
				kbytes >> 1, kbytes << 1,
				kbytes - 1, kbytes + 1,
				kbytes - 64, kbytes + 64);
			break;
		case 2:
			mode = (mode + 1) % 3;
			break;
		case 4:
			xmb_offset = (offset_from_end ^ 0xFFFF) - (kbytes - 1);
			xmb_mode = mode;

			outportb(IO_CART_FLASH, 0x01);

			xmodem_run_recv(xmf_write, xmf_erase_finish, kbytes, 8, true);
			xmodem_run_recv(xmf_write, xmf_write_finish, kbytes, 8, false);

			outportb(IO_CART_FLASH, 0x00);
			break;
		case 5:
			return;
		}
	}
}

uint16_t menu_show_main(void) {
	menu_state_t state;
	menu_entry_t entries[6];
	uint8_t entry_count = 0;

	entries[entry_count].text = msg_send_ipl;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_backup;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_restore;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_erase;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_flash;
	entries[entry_count++].flags = 0;
	entries[entry_count].text = msg_baud_38400;
	entries[entry_count++].flags = 0;
	state.entries = entries; state.entry_count = entry_count;
	ui_menu_init(&state);

	uint16_t result;
	bool active = true;
	while (active) {	
		entries[entry_count - 1].text = (xm_baudrate == SERIAL_BAUD_38400) ? msg_baud_38400 : msg_baud_9600;
		result = ui_menu_run(&state, 3 + ((14 - entry_count) >> 1));
		if (result == entry_count - 1) {
			xm_baudrate ^= SERIAL_BAUD_38400;
		} else {
			active = false;
		}
	}
	ui_puts(0, 0, COLOR_YELLOW, msg_none); // TODO: compiler error workaround?
	return result;
}

void menu_main(void) {
	input_wait_clear();
	uint16_t result = menu_show_main();
	switch (result) {
	case 0: // IPL transfer
		xmodem_run_send(xmb_ipl_read, 64, 1);
		break;
	case 1: // Cart Backup
		menu_backup(false, false);
		break;
	case 2: // Cart Restore
		menu_backup(true, false);
		break;
	case 3: // Cart Erase
		menu_backup(true, true);
		break;
	case 4: // Cart Flash
		menu_flash();
		break;
	default:
		break;
	}
}

static const char __far msg_title[] = "-= WS Backup Tool v0.1 =-";

int main(void) {
	cpu_irq_disable();
	xm_baudrate = SERIAL_BAUD_38400;

	ui_init();

	outportb(IO_HWINT_ACK, 0xFF);
	ws_hwint_set_handler(HWINT_IDX_VBLANK, vblank_int_handler);
	ws_hwint_enable(HWINT_VBLANK);
	cpu_irq_enable();

	ui_puts(2, 1, 0, msg_title);

	while(1) menu_main();
}
