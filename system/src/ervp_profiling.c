#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "ervp_assert.h"
#include "ervp_misc_util.h"
#include "ervp_printf.h"
#include "ervp_real_clock.h"
#include "ervp_profiling.h"
#include "core_dependent.h"
#include "ervp_per_core.h"
#include "ervp_lock.h"

#ifdef INCLUDE_MCYCLE
#define PROFILING_TICK_HZ CORE_CLK_HZ
static inline uint64_t get_profiling_tick() { return get_mcycle(); }
#else
#define PROFILING_TICK_HZ TICK_HZ
static inline uint64_t get_profiling_tick() { return get_real_clock_tick(); }
#endif

#ifdef USE_PROFILING

static const int RECORDING_IDLE = 0;
static const int RECORDING_BUSY = 1;

static profile_t *section_table PER_CORE_DATA = NULL;
static int profile_id PER_CORE_DATA = 0;

static const int MIN_ACCURATE_TICK = 10;
static float min_duration_ms PER_CORE_DATA = 0;

static inline int __enter_profiling()
{
	int interrupt_status = is_interrupt_trap_enabled();
	if (interrupt_status)
		disable_interrupt_trap();
	return interrupt_status;
}

static inline void __exit_profiling(int interrupt_status)
{
	if (interrupt_status)
		enable_interrupt_trap();
}

static void profiling_warning()
{
#ifndef SILENT_FOR_PROFILING
	printf_must("\n[RVX/PROFILING/WARNING] includes printf time\n");
#endif
}

void profiling_init()
{
	for (int i = 0; i < 3; i++)
	{
		profiling_start("__profiling_delay");
		profiling_end("__profiling_delay");
	}
	profile_t *section = PER_CORE(section_table);
	float average_ms, total_ms;
	total_ms = ((float)section->num_tick_rshifted) / ((PROFILING_TICK_HZ / 1000) >> section->shift_amount);
	average_ms = total_ms / section->num_called;
	PER_CORE(min_duration_ms) = average_ms * 5;
	if (PER_CORE(min_duration_ms) < ((float)MIN_ACCURATE_TICK) / (PROFILING_TICK_HZ / 1000))
		PER_CORE(min_duration_ms) = ((float)MIN_ACCURATE_TICK) / (PROFILING_TICK_HZ / 1000);
	// HASH_DEL(_section_table, section);
}

static void _profiling_section_print(const profile_t *section, float min_accurate_ms)
{
	float average_ms, total_ms;
	printf_must("\n\n[section] %s", section->name);
	printf_must("\ntotal tick %llu<<%u", section->num_tick_rshifted, section->shift_amount);
	printf_must("\nnum_called %u", section->num_called);
	if (section->num_called == 0)
	{
		printf_must("\ntotal time (ms) N/A");
		printf_must("\naverage time (ms) N/A");
		return;
	}
	total_ms = ((float)section->num_tick_rshifted) / ((PROFILING_TICK_HZ / 1000) >> section->shift_amount);
	average_ms = total_ms / section->num_called;
	printf_must("\ntotal time (ms) %.2f", total_ms);
	printf_must("\naverage time (ms) %.4f", average_ms);
	if (average_ms <= min_accurate_ms)
	{
		printf_must("\n[RVX/PROFILING/WARNING] NOT accurate due to short running time");
	}
}

static inline void __section_init(profile_t *section)
{
	section->id = PER_CORE(profile_id)++;
	section->num_called = 0;
	section->state = RECORDING_IDLE;
	section->num_tick_rshifted = 0;
	section->shift_amount = 0;
}

static profile_t *__profiling_register(const char *name)
{
	profile_t *section;
	section = (profile_t *)malloc(sizeof(profile_t));
	assert(section);
	__section_init(section);
	section->name = malloc(sizeof(char) * (strlen(name) + 1));
	strcpy(section->name, name);
	HASH_ADD_STR(PER_CORE(section_table), name, section);
	return section;
}

profile_t *profiling_register(const char *name)
{
	profile_t *section;
	int interrupt_status = __enter_profiling();
	HASH_FIND_STR(PER_CORE(section_table), name, section);
	assert(section == NULL);
	section = __profiling_register(name);
	__exit_profiling(interrupt_status);
	return section;
}

static void __profiling_start(profile_t *section)
{
	assert(section);
	assert_msg(section->state != RECORDING_BUSY, "profiling pair is NOT matched %s", section->name);
	section->num_called++;
	section->state = RECORDING_BUSY;
	section->previous_tick = get_profiling_tick();
}

profile_t *profiling_start_by_name(const char *name)
{
	profile_t *section;
	int interrupt_status = __enter_profiling();
	HASH_FIND_STR(PER_CORE(section_table), name, section);
	if (section == NULL)
		section = __profiling_register(name);
	__profiling_start(section);
	__exit_profiling(interrupt_status);
	return section;
}

static void __profiling_end(profile_t *section, uint64_t current_tick)
{
	uint64_t diff;
	assert(section);
	assert_msg(section->state == RECORDING_BUSY, "profiling pair is NOT matched %s", section->name);
	section->state = RECORDING_IDLE;
	diff = (current_tick - section->previous_tick) >> section->shift_amount;
	section->num_tick_rshifted += diff;
	if ((section->num_tick_rshifted >> 62) > 0)
	{
		section->num_tick_rshifted >>= 1;
		section->shift_amount++;
	}
}

void profiling_end_by_name(const char *name)
{
	uint64_t current_tick = get_profiling_tick();
	profile_t *section;
	int interrupt_status = __enter_profiling();
	HASH_FIND_STR(PER_CORE(section_table), name, section);
	__profiling_end(section, current_tick);
	__exit_profiling(interrupt_status);
}

void profiling_start_by_section(profile_t *section)
{
	int interrupt_status = __enter_profiling();
	__profiling_start(section);
	__exit_profiling(interrupt_status);
}

void profiling_end_by_section(profile_t *section)
{
	uint64_t current_tick = get_profiling_tick();
	int interrupt_status = __enter_profiling();
	__profiling_end(section, current_tick);
	__exit_profiling(interrupt_status);
}

static int compare_name(const profile_t *a, const profile_t *b)
{
	return strcmp(a->name, b->name);
}

void profiling_print()
{
	int num_table = 0;
	for (int core_index = 0; core_index < NUM_CORE_PHYSICAL; core_index++)
		if (PER_CORE_OF(section_table, core_index) != NULL)
			num_table++;
#ifdef LOCK_INDEX_FOR_PRINTF
	const int interrupt_status = acquire_shared_resource(LOCK_INDEX_FOR_PRINTF);
#endif
	printf_must("\n");
	profiling_warning();
	for (int core_index = 0; core_index < NUM_CORE_PHYSICAL; core_index++)
	{
		if (PER_CORE_OF(section_table, core_index) == NULL)
			continue;
		if (num_table > 1)
			printf_must("\n\n[core %d]", core_index);
		HASH_SORT(PER_CORE_OF(section_table, core_index), compare_name);
		for (profile_t *section = PER_CORE_OF(section_table, core_index); section != NULL; section = (profile_t *)(section->hh.next))
		{
			char section_name_prefix[3];
			section_name_prefix[0] = section->name[0];
			section_name_prefix[1] = section->name[1];
			section_name_prefix[2] = 0;
			if (strcmp(section_name_prefix, "__") != 0)
				_profiling_section_print(section, PER_CORE_OF(min_duration_ms, core_index));
		}
	}
#ifdef LOCK_INDEX_FOR_PRINTF
	release_shared_resource(LOCK_INDEX_FOR_PRINTF, interrupt_status);
#endif
}

#endif