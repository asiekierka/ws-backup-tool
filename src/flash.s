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

#include <wonderful.h>

	.arch	i186
	.code16
	.intel_syntax noprefix
	.global flash_write
	.global flash_erase

	.align 2
_driver_reset_flash:
	push ds
	mov ax, 0x1000
	mov ds, ax
	mov byte ptr [0xAAAA], 0xAA
	mov byte ptr [0x5555], 0x55
	mov byte ptr [0xAAAA], 0xF0
	pop ds
	ret

	.align 2
_flash_write_busyloop:
	nop
	nop
	mov al, byte ptr es:[di]
	nop
	nop
	cmp al, byte ptr es:[di]
	jne _flash_write_busyloop
	ret

	.align 2
flash_write:
	push	si
	push	di
	push	ds
	push	es
	push    bp
	mov     bp, sp

	mov si, ax
	mov di, dx
	
	xor bx, bx
	mov ds, bx
	mov bx, 0x1000
	mov es, bx
	mov bx, 0xAAAA

	mov ax, [bp + IA16_CALL_STACK_OFFSET(10)]
	cmp al, 3
	je flash_write_fast_mx29l
	cmp al, 2
	je flash_write_fast_flashmasta
	cmp al, 1
	je flash_write_fast_wonderwitch

flash_write_slow:
	cld
	.balign 2, 0x90
flash_write_slow_loop:
	mov byte ptr es:[bx], 0xAA
	nop
	mov byte ptr es:[0x5555], 0x55
	nop
	mov byte ptr es:[bx], 0xA0
	nop
	movsb
	call _flash_write_busyloop
	loop flash_write_slow_loop // 5 cycles

	push es
	pop ds

	jmp flash_write_end

	// === MX29L ===

flash_write_fast_mx29l:
	mov byte ptr es:[bx], 0xAA
	mov byte ptr es:[0x5555], 0x55
	mov byte ptr es:[bx], 0xA0

	push di
	cld
	rep movsb

	// wait at least 100us
	mov cx, 80
flash_write_fast_mx29l_loop:
	nop
	loop flash_write_fast_mx29l_loop

	pop di

flash_write_fast_mx29l_wait:
	mov al, byte ptr es:[di]
	test al, 0x80
	jz flash_write_fast_mx29l_wait

	push es
	pop ds

	mov byte ptr es:[bx], 0xAA
	mov byte ptr es:[0x5555], 0x55
	mov byte ptr es:[bx], 0xF0

	jmp flash_write_end

	// === WonderWitch (MBM29DL400BC) ===

flash_write_fast_wonderwitch:
	// start bypass
	mov byte ptr es:[bx], 0xAA
	mov byte ptr es:[0x5555], 0x55
	mov byte ptr es:[bx], 0x20

	xor bx, bx
	cld
	.balign 2, 0x90
flash_write_fast_wonderwitch_loop:
	mov byte ptr es:[bx], 0xA0
	movsb
	call _flash_write_busyloop
	loop flash_write_fast_wonderwitch_loop // 5 cycles

	push es
	pop ds

	// stop bypass
	mov byte ptr [bx], 0x90
	mov byte ptr [bx], 0xF0

	jmp flash_write_end

	// === WSFM (JS28F00) ===

flash_write_fast_flashmasta:
	// start bypass
	mov byte ptr es:[bx], 0xAA
	mov byte ptr es:[0x5555], 0x55
	mov byte ptr es:[bx], 0x20

	xor bx, bx
	cld
	.balign 2, 0x90
flash_write_fast_flashmasta_loop:
	mov byte ptr es:[bx], 0xA0
	movsb
	call _flash_write_busyloop
	loop flash_write_fast_flashmasta_loop // 5 cycles

	push es
	pop ds

	// stop bypass
	mov byte ptr [bx], 0x90
	mov byte ptr [bx], 0x00

	// reset
	mov byte ptr [0xAAAA], 0xAA
	mov byte ptr [0x5555], 0x55
	mov byte ptr [0xAAAA], 0xF0

flash_write_end:
	pop	bp
	pop	es
	pop	ds
	pop	di
	pop	si

	IA16_RET 0x2

	.align 2
flash_erase:
	push ds
	push si

	// execute erase command
	mov bx, 0x1000
	mov ds, bx
	mov bx, 0xAAAA
	mov si, 0x5555

	mov byte ptr [bx], 0xAA
	mov byte ptr [si], 0x55
	mov byte ptr [bx], 0x80
	mov byte ptr [bx], 0xAA
	mov byte ptr [si], 0x55
	mov bx, ax
	mov byte ptr [bx], 0x30

	.balign 2, 0x90
flash_erase_busyloop:
	nop
	nop
	nop
	mov al, byte ptr [bx]
	nop
	nop
	nop
	cmp al, byte ptr [bx] // DQ2 and/or DQ6 toggles if status register
	jne flash_erase_busyloop

	pop si
	pop ds

	IA16_RET
