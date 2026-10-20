#ifndef __ERVP_PER_CORE_H__
#define __ERVP_PER_CORE_H__

#include <stdint.h>
#include "ervp_variable_allocation.h"

extern uintptr_t _percore_start_;
extern uintptr_t _percore_end_;
extern uintptr_t _percore_area_start_;

static inline uintptr_t __get_per_core_offset()
{
	uintptr_t offset;
	__asm__("mv %0, tp" : "=r"(offset));
	return offset;
}

static inline uintptr_t __get_per_core_offset_of(int core_index)
{
	const uintptr_t start = (uintptr_t)(&_percore_start_);
	const uintptr_t block_size = ((uintptr_t)(&_percore_end_)) - start;
	return ((uintptr_t)(&_percore_area_start_)) + (core_index * block_size) - start;
}

#define PER_CORE(var) (*(__typeof__(&(var)))(((uintptr_t)(&(var))) + __get_per_core_offset()))
#define PER_CORE_OF(var, core_index) (*(__typeof__(&(var)))(((uintptr_t)(&(var))) + __get_per_core_offset_of(core_index)))

void _init_per_core();

#endif
