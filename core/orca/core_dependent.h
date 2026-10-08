#ifndef __CORE_DEPENDENT_H__
#define __CORE_DEPENDENT_H__

#include <stdint.h>

#include "platform_info.h"

#include "riscv_isa.h"
#include "csr_encoding.h"
#include "ervp_reg_util.h"
#include "ervp_interrupt.h"

#define BIG_ENDIAN_ACCESS 0

static inline int get_local_id()
{
	int result;
	if(NUM_CORE_PER_CORECLUSTER==1)
		result = 0;
	else
		while(1);
	return result;
}

static inline unsigned int is_interrupt_trap_enabled(){
	return ((read_csr(mstatus) & MSTATUS_MIE)>0);
}

static inline void enable_interrupt_trap(){
	// Pending interrupts (mip & mie) now cause a trap.
	unsigned int pending = read_csr(mip) & read_csr(mie);
	set_csr(mstatus, MSTATUS_MIE);
	while((read_csr(mip) & pending)!=0);
}

static inline void disable_interrupt_trap(){
	// Interrupts stay pending in mip but cause no trap.
	clear_csr(mstatus, MSTATUS_MIE);
}

static inline void wait_for_interrupt(){
	asm volatile ("nop");
}

static inline void allow_interrupt_timer(){
	// Enable the Machine-Timer bit in MIE
	set_csr(mie, MIP_MTIP);
}

static inline void disallow_interrupt_timer(){
	// Disable the Machine-Timer bit in MIE
	clear_csr(mie, MIP_MTIP);
}

static inline void allow_interrupt_plic(){
	check_plic_grant();
	// Enable the Machine-External bit in MIE
	set_csr(mie, MIP_MEIP);
}

static inline void disallow_interrupt_plic(){
	// Disable the Machine-External bit in MIE
	clear_csr(mie, MIP_MEIP);
}

static inline void flush_cache(){}

/*
static inline void disable_interrupt_trap(){
	int value;
	value = 0;
	write_csr(MEIMASK, value);
	value = 0;
	write_csr(MSTATUS, value);
}
static inline void enable_interrupt_trap(){
	int value;
	value = 0xFFFFFFFF;
	write_csr(MEIMASK, value);
	value = 8;
	write_csr(MSTATUS, value);
}
*/
static inline void init_core()
{
	write_csr(mie, 0);
}
static inline void init_cache(uintptr_t cacheable_start, uintptr_t cacheable_last){}

#endif
