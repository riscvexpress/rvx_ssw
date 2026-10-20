#include <stdint.h>
#include "platform_info.h"

#include "ervp_misc_util.h"
#include "ervp_mmio_util.h"
#include "ervp_common_peri_group_memorymap.h"
#include "ervp_printf.h"
#include "ervp_real_clock.h"

_Static_assert((TICK_HZ % 1000000) == 0, "real clock tick must be a multiple of 1MHz");

static const unsigned int TICK_PER_US = TICK_HZ / 1000000;
static const unsigned int TICK_PER_MS = TICK_HZ / 1000;

static inline uint32_t __get_real_clock_tick32()
{
	return REG32(MMAP_SUBMODULE_REAL_CLOCK);
}

static inline uint64_t __get_real_clock_tick()
{
	volatile uint32_t* addr = (uint32_t*)(MMAP_SUBMODULE_REAL_CLOCK);
	uint32_t upper, lower;
	do
	{
		upper = addr[1];
		lower = addr[0];
	} while (upper != addr[1]);
	return (((uint64_t)upper) << 32) | lower;
}

uint64_t get_real_clock_tick()
{
	uint64_t value = __get_real_clock_tick();
	return value;
}

uint32_t get_real_clock_tick32()
{
	return __get_real_clock_tick32();
}

void print_real_clock_tick()
{
	printf("\n%llu", get_real_clock_tick());
}

static inline void __delay_tick(uint64_t num_tick)
{
	if (num_tick < 0x80000000)
	{
		const uint32_t start_tick = __get_real_clock_tick32();
		while ((__get_real_clock_tick32() - start_tick) <= num_tick);
	}
	else
	{
		const uint64_t start_tick = __get_real_clock_tick();
		while ((__get_real_clock_tick() - start_tick) <= num_tick);
	}
}

void delay_us_by_real_clock(unsigned int time_in_us)
{
	__delay_tick(((uint64_t)time_in_us) * TICK_PER_US);
}

void delay_ms_by_real_clock(unsigned int time_in_ms)
{
	__delay_tick(((uint64_t)time_in_ms) * TICK_PER_MS);
}

void delay_sec_by_real_clock(unsigned int time_in_sec)
{
	__delay_tick(((uint64_t)time_in_sec) * TICK_HZ);
}

void print_real_clock_dhms()
{
  uint64_t tick = __get_real_clock_tick();
  uint64_t ms, sec, min, hour, day;
  int ms_trunc, sec_trunc, min_trunc, hour_trunc, day_trunc;

  ms = tick / TICK_PER_MS;
  sec = ms / 1000;
  min = sec / 60;
  hour = min / 60;
  day = hour / 24;

  ms_trunc = ms % 1000;
  sec_trunc = sec % 60;
  min_trunc = min % 60;
  hour_trunc = hour % 24;
  day_trunc = day;
  printf("\nd%03d:h%02d:m%02d:s%02d:ms%03d", day_trunc, hour_trunc, min_trunc, sec_trunc, ms_trunc);
}
