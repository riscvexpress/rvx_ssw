#include "ervp_platform_controller_memorymap.h"
#include "ervp_external_peri_group_memorymap.h"
#include "platform_info.h"

boot_start:
  set_static_register:
    // x0: zero
    //li x0, 0
    // s0: PLATFORM_CONTROLLER_BASEADDR
    li s0, PLATFORM_CONTROLLER_BASEADDR
    // s1: not used
    // s2: spi_baseaddr
    li s2, (EXTERNAL_PERI_GROUP_BASEADDR+SUBMODULE_ADDR_ERVP_EXTERNAL_PERI_GROUP_SPI)
    // s3: number of bytes to read
		// s4: memory addr to write
    // s6: backup of return address
    // s7 : addr of boot status
    li s7, MMAP_OFFSET_PLATFORM_REGISTER_BOOT_STATUS
    add s7, s0, s7
  check_boot_mode:
    li t0, MMAP_OFFSET_PLATFORM_REGISTER_INITIALIZED
    add t0, s0, t0
1:  lw t1, 0(t0)
    beqz t1, 1b
    li t0, MMAP_OFFSET_PLATFORM_REGISTER_BOOT_MODE
    add t0, s0, t0
    lw t1, 0(t0)
    li t2, BOOT_MODE_OCD
    beq t1, t2, start_app
    li t2, BOOT_MODE_STAND_ALONE
    beq t1, t2, load_app_from_flash
    j infinite_loop
  start_app:
    li t2, BOOT_STATUS_ALL_READY
1:  lw t1, 0(s7)
    bne t1, t2, 1b
    li t0, MMAP_OFFSET_PLATFORM_REGISTER_APP_ADDR
    add t0, s0, t0
    lw t0, 0(t0)
    jr t0
  infinite_loop:
    j infinite_loop

load_app_from_flash:
  be_unique:
    lw t1, 0(s7)
    bne x0, t1, start_app
#ifdef INCLUDE_OFFCHIP_MRAM
    j set_ready
#endif
  configure_spi:
    li t2, 0xA0000000
    sw t2, 0x10(s2)
    li t2, NUM_SPI-1
    sw t2, 0x38(s2)
    slli t1, t2, 2
    add t1, t1, s2
    li t2, 0xC0000004
    sw t2, 0x18(t1)
  set_flash_baseaddr:
    li t0, MMAP_PLATFORM_REGISTER_FLASH_BINARY_BASEADDR
    lw a0, 0(t0)
  align_flash_addr:
    addi a0, a0, 3
    andi t0, a0, 3
    sub a0, a0, t0
  num_bytes:
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
		jal convert_endian
		beqz a1, set_ready
		mv s3, a1
	memory_addr:
		jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
		jal convert_endian
		mv s4, a1
  write_unit:
		jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
		jal convert_endian
    li t1, 2
    beq a1, t1, loop_2byte
    li t1, 4
    beq a1, t1, loop_4byte
  loop_1byte:
		li a1, 0
    jal read_one_byte_from_flash
		sb a1, 0(s4)
    addi s3, s3, -1
		addi s4, s4, 1
    bnez s3, loop_1byte
		j align_flash_addr
  loop_2byte:
		li a1, 0
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal convert_endian
    srli a1, a1, 16
		sh a1, 0(s4)
    addi s3, s3, -2
		addi s4, s4, 2
    bnez s3, loop_2byte
		j align_flash_addr
  loop_4byte:
		li a1, 0
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal read_one_byte_from_flash
    jal convert_endian
		sw a1, 0(s4)
    addi s3, s3, -4
		addi s4, s4, 4
    bnez s3, loop_4byte
		j align_flash_addr
  set_ready:
    sw x0, 0(s7)
    j start_app

read_one_byte_from_flash: // a0: flash addr to read, a1: value
    mv s6, ra
    jal wait_ready
  cmd:
    srli t0, a0, 24
    slli t0, t0, 8
    ori t0, t0, 0x13
    srli t1, a0, 16
    andi t1, t1, 0xFF
    slli t1, t1, 16
    or t0, t0, t1
    srli t1, a0, 8
    slli t1, t1, 24
    or t0, t0, t1
    sw t0, 0x44(s2)
    andi t0, a0, 0xFF
    sw t0, 0x44(s2)
    li t2, 0x2204
    sw t2, 0x3C(s2)
    jal wait_ready
  read_rx_fifo:
    li t2, 0x1000
    sw t2, 0x3C(s2)
    li t1, 0x1000000
1:  lw t0, 0x14(s2)
    and t0, t0, t1
    bnez t0, 1b
    lw t0, 0x40(s2)
    andi t0, t0, 0xFF
  accum:
    slli a1, a1, 8
    add a1, a1, t0
  incr:
    addi a0, a0, 1
    mv ra, s6
    ret

wait_ready:
    lw t2, 0x14(s2)
    bgez t2, wait_ready
    ret

convert_endian: // a1: value
	li t0, 0xFF
	and t1, a1, t0
	srli a1, a1, 8
	and t2, a1, t0
	srli a1, a1, 8
	and t3, a1, t0
	srli a1, a1, 8 // t4
	slli t3, t3, 8
	or a1, a1, t3
	slli t2, t2, 16
	or a1, a1, t2
	slli t1, t1, 24
	or a1, a1, t1
	ret

/*
copy_one_byte_from_flash_to_memory: // a0: flash addr to read, a1: memory addr to write
    mv s6, ra
  enable_spi:
    li t2, 2
    sw t2, 0x18(s2)
  set_write_direction:
    li t2, 0x00080008
    sw t2, 0x40(s2)
  cmd:
    li a2, 3
    jal write_tx_fifo
    srli a2, a0, 16
    jal write_tx_fifo
    srli a2, a0, 8
    jal write_tx_fifo
    mv a2, a0
    jal write_tx_fifo
  set_read_direction:
    li t2, 0x00080000
    sw t2, 0x40(s2)
  read_rx_fifo:
    jal write_tx_fifo
1:  lw t0, 0x4c(s2)
    srli t1,t0,31
    bnez t1, 1b
    sb t0, 0(a1)
  disable_spi:
    li t2, 3
    sw t2, 0x18(s2)
  incr:
    addi a0, a0, 1
    addi a1, a1, 1
    mv ra, s6
    ret
*/
