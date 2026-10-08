#include "platform_info.h"
#include "ervp_malloc.h"
#include "ervp_variable_allocation.h"
#include "npx_spm.h"

#include "utlist.h"

#if defined(USE_NPX_SPM)

typedef struct npx_spm_block npx_spm_block_t;

struct npx_spm_block
{
	uintptr_t ptr;
	size_t size;
	npx_spm_block_t *prev;
	npx_spm_block_t *next;
	int available;
};

static uintptr_t BUFFER_START_ADDR = 0;
static size_t BUFFER_MAX_SIZE = 0;
static npx_spm_block_t *npx_spm_allocator NOTCACHED_DATA = NULL;

__attribute__((weak)) void npx_spm_create(uintptr_t baseaddr, size_t size)
{
	assert(npx_spm_allocator == NULL);
	BUFFER_START_ADDR = baseaddr;
	BUFFER_MAX_SIZE = size;

	npx_spm_block_t *block = malloc(sizeof(npx_spm_block_t));
	block->ptr = baseaddr;
	block->size = size;
	block->available = 1;
	// DL_APPEND_ELEM(npx_spm_allocator, npx_spm_allocator, block);
	DL_APPEND(npx_spm_allocator, block);
}

static void npx_spm_print_block(npx_spm_block_t *temp, int index)
{
	printf("\n[%d] %08x, %8x, %1d", index, temp->ptr, temp->size, temp->available);
}

__attribute__((weak)) void npx_spm_print_all()
{
	npx_spm_block_t *temp;
	int i = 0;
	DL_FOREACH(npx_spm_allocator, temp)
	{
		npx_spm_print_block(temp, i++);
	}
}

__attribute__((weak)) void npx_spm_destroy()
{
	assert(npx_spm_allocator);
	npx_spm_block_t *temp;
	int count;
	DL_COUNT(npx_spm_allocator, temp, count);
	if (count != 1)
	{
		npx_spm_print_all();
		assert_msg(0, "some npx buffers are NOT freed");
	}
	DL_DELETE(npx_spm_allocator, npx_spm_allocator);
	npx_spm_allocator = NULL;
}

static int lack_amount NOTCACHED_DATA = 0;

__attribute__((weak)) void npx_spm_report()
{
#ifndef NDEBUG
	printf_function();
	printf("lack_amount: %d", lack_amount);
#endif
}

__attribute__((weak)) void *npx_spm_malloc(size_t size)
{
	assert(size);
	void *result;
	if (npx_spm_allocator)
	{
		size = ALIGN_UP_POW2(size, CACHE_LINE_SIZE);
		npx_spm_block_t *best_block = NULL;
		npx_spm_block_t *temp;
		DL_FOREACH(npx_spm_allocator, temp)
		{
			if (temp->available)
			{
				if (temp->size >= size)
				{
					if (best_block == NULL)
						best_block = temp;
					else if (best_block->size > temp->size)
						best_block = temp;
				}
			}
		}

		result = NULL;
		if (best_block)
		{
			npx_spm_block_t *remain;
			remain = malloc(sizeof(npx_spm_block_t));
			remain->ptr = best_block->ptr + size;
			remain->size = best_block->size - size;
			remain->available = 1;

			best_block->size = size;
			best_block->available = 0;

			DL_APPEND_ELEM(npx_spm_allocator, best_block, remain);

			result = (void *)best_block->ptr;
		}
		else
		{
#ifndef NDEBUG
			printf("\nBuffer is not enough : %d", size);
			if (size > lack_amount)
				lack_amount = size;
#endif
		}
	}
	else
		result = NULL;

	if (result)
		printf_function();

	return result;
}

static inline int is_npx_spm_addr(uintptr_t ptr)
{
	int valid = 0;
	if (npx_spm_allocator)
	{
		if ((ptr >= BUFFER_START_ADDR) && (ptr < (BUFFER_START_ADDR + BUFFER_MAX_SIZE)))
			valid = 1;
	}
	return valid;
}

__attribute__((weak)) int npx_spm_free(void *ptr)
{
	int success = is_npx_spm_addr((uintptr_t)ptr);
	if (success)
	{
		printf_function();
		int found = 0;
		npx_spm_block_t *temp;
		DL_FOREACH(npx_spm_allocator, temp)
		{
			if (temp->ptr == ptr)
			{
				// an spm address must be the start of a block that is still in use
				assert_must(temp->available == 0);
				found = 1;
				int merge_to_next;
				int merge_to_prev;
				merge_to_next = temp->next && temp->next->available;
				merge_to_prev = (temp != npx_spm_allocator) && temp->prev->available;

				if (merge_to_next && merge_to_prev)
				{
					npx_spm_block_t *update_block = temp->prev;
					npx_spm_block_t *delete1_block = temp->next;
					npx_spm_block_t *delete2_block = temp;
					update_block->size += temp->size;
					update_block->size += temp->next->size;

					DL_DELETE(npx_spm_allocator, delete1_block);
					DL_DELETE(npx_spm_allocator, delete2_block);
					free(delete1_block);
					free(delete2_block);
				}
				else if (merge_to_next)
				{
					npx_spm_block_t *update_block = temp->next;
					npx_spm_block_t *delete_block = temp;
					update_block->size += temp->size;
					update_block->ptr = temp->ptr;
					DL_DELETE(npx_spm_allocator, delete_block);
					free(delete_block);
				}
				else if (merge_to_prev)
				{
					npx_spm_block_t *update_block = temp->prev;
					npx_spm_block_t *delete_block = temp;
					update_block->size += temp->size;
					DL_DELETE(npx_spm_allocator, delete_block);
					free(delete_block);
				}
				else
				{
					temp->available = 1;
				}
				break;
			}
		}
		assert_must(found);
	}
	return success;
}

#endif