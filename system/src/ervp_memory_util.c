#include <string.h>
#include "platform_info.h"

#include "ervp_printf.h"
#include "ervp_mmio_util.h"
#include "ervp_round_int.h"
#include "ervp_assert.h"
#include "ervp_core_id.h"

void print_memory_byte(uint8_t *array, int num)
{
	int i;
	for (i = 0; i < num; i++)
	{
		if ((i & 7) == 0)
			printf("\n");
		printf("0x%02x ", array[i]);
	}
}

void print_memory_4byte(uint32_t *array, int num)
{
	int i;
	assert_pointer_align(array, 4);
	for (i = 0; i < num; i++)
	{
		if ((i & 3) == 0)
			printf("\n");
		printf("0x%08x ", (uintptr_t)(array[i]));
	}
}

static void _print_first_diff(const void *result, const void *ref, size_t num)
{
	const uint8_t *result_temp = (const uint8_t *)result;
	const uint8_t *ref_temp = (const uint8_t *)ref;
	for (size_t i = 0; i < num; i++)
	{
		if (result_temp[i] != ref_temp[i])
		{
			printf("\n[0x%08x] 0x%02x 0x%02x", (const unsigned int)(result_temp + i), result_temp[i], ref_temp[i]);
			break;
		}
	}
}

int memory_equal(const void *result, const void *ref, size_t num, int prints)
{
	const int all_are_equal = (memcmp(result, ref, num) == 0);
	if (prints)
	{
		if (all_are_equal)
			printf("\nall correct");
		else
		{
			_print_first_diff(result, ref, num);
			printf("\nsome incorrect");
		}
	}
	return all_are_equal;
}
