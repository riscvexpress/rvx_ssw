#ifndef __ERVP_MCYCLE_H__
#define __ERVP_MCYCLE_H__

#include <stdint.h>
#include "riscv_isa.h"

#ifdef INCLUDE_MCYCLE

static inline uint32_t get_mcycle32()
{
	return read_csr(mcycle);
}

static inline uint64_t get_mcycle()
{
	uint32_t upper, lower;
	do
	{
		upper = read_csr(mcycleh);
		lower = read_csr(mcycle);
	} while (upper != read_csr(mcycleh));
	return (((uint64_t)upper) << 32) | lower;
}

#endif

#endif
