#ifndef __ERVP_DELAY_H__
#define __ERVP_DELAY_H__

#include "platform_info.h"
#include "ervp_assert.h"
#include "core_dependent.h"

#ifdef INCLUDE_MCYCLE

#define SHORT_DELAY_US 20

static inline void delay_cycle(unsigned int num_cycle)
{
	const uint32_t start_cycle = get_mcycle32();
	while ((get_mcycle32() - start_cycle) < num_cycle);
}

static inline void delay_ns(unsigned int time_in_ns)
{
	const unsigned int cycle_per_ns_q32 = (unsigned int)((((uint64_t)CORE_CLK_HZ) << 32) / 1000000000) + 1;
	delay_cycle((unsigned int)((((uint64_t)time_in_ns) * cycle_per_ns_q32) >> 32) + 1);
}

static inline void __delay_cycle_long(uint64_t num_cycle)
{
	if (num_cycle < 0x80000000)
		delay_cycle(num_cycle);
	else
	{
		const uint64_t start_cycle = get_mcycle();
		while ((get_mcycle() - start_cycle) < num_cycle);
	}
}

static inline void delay_us_by_mcycle(unsigned int time_in_us)
{
	__delay_cycle_long(((uint64_t)time_in_us) * (CORE_CLK_HZ / 1000000));
}

static inline void delay_ms_by_mcycle(unsigned int time_in_ms)
{
	__delay_cycle_long(((uint64_t)time_in_ms) * (CORE_CLK_HZ / 1000));
}

static inline void delay_sec_by_mcycle(unsigned int time_in_sec)
{
	__delay_cycle_long(((uint64_t)time_in_sec) * CORE_CLK_HZ);
}

#endif

#if defined(INCLUDE_TIMER)

#include "ervp_timer.h"

static inline void delay_us(unsigned int time_in_us)
{
#ifdef INCLUDE_MCYCLE
	if (time_in_us < SHORT_DELAY_US)
	{
		delay_us_by_mcycle(time_in_us);
		return;
	}
#endif
	delay_us_by_timer(time_in_us);
}

static inline void delay_ms(unsigned int time_in_ms)
{
	delay_ms_by_timer(time_in_ms);
}

static inline void delay_sec(unsigned int time_in_sec)
{
	delay_sec_by_timer(time_in_sec);
}

#elif defined(INCLUDE_MCYCLE)

static inline void delay_us(unsigned int time_in_us)
{
	delay_us_by_mcycle(time_in_us);
}

static inline void delay_ms(unsigned int time_in_ms)
{
	delay_ms_by_mcycle(time_in_ms);
}

static inline void delay_sec(unsigned int time_in_sec)
{
	delay_sec_by_mcycle(time_in_sec);
}

#else

#include "ervp_real_clock.h"

static inline void delay_us(unsigned int time_in_us)
{
	delay_us_by_real_clock(time_in_us);
}

static inline void delay_ms(unsigned int time_in_ms)
{
	delay_ms_by_real_clock(time_in_ms);
}

static inline void delay_sec(unsigned int time_in_sec)
{
	delay_sec_by_real_clock(time_in_sec);
}

#endif

#endif
