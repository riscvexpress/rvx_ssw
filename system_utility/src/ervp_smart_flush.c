#include "ervp_smart_flush.h"

#include "ervp_misc_util.h"
#include "ervp_printf.h"
#include "ervp_caching.h"
#include "ervp_memory_allocator.h"
#include "ervp_variable_allocation.h"
#include <stdarg.h>

#if defined(USE_NPX)
#include "npx_spm.h"
#endif

char trackedvar_track_enable[NUM_CORE] DATA_BSS;

#ifdef USE_SMART_FLUSH

static utset trackedvar_track[NUM_CORE] DATA_BSS;
static memory_allocator_t smart_allocator[NUM_CORE] DATA_BSS;

static unsigned short free_list_max_size[NUM_CORE] DATA_BSS;
static unsigned short free_list_current_size[NUM_CORE] DATA_BSS;
static void **delayed_free_list[NUM_CORE] DATA_BSS;

static inline void increase_delayed_free_list()
{
  if (free_list_max_size[EXCLUSIVE_ID] == 0)
  {
    free_list_max_size[EXCLUSIVE_ID] = 16;
    delayed_free_list[EXCLUSIVE_ID] = malloc(free_list_max_size[EXCLUSIVE_ID] * sizeof(void *));
  }
  else
  {
    void *old_ptr = delayed_free_list[EXCLUSIVE_ID];
    int old_size = free_list_max_size[EXCLUSIVE_ID] * sizeof(void *);
    free_list_max_size[EXCLUSIVE_ID] = free_list_max_size[EXCLUSIVE_ID] << 1;
    delayed_free_list[EXCLUSIVE_ID] = malloc(free_list_max_size[EXCLUSIVE_ID] * sizeof(void *));
    memcpy_rvx(delayed_free_list[EXCLUSIVE_ID], old_ptr, old_size);
    free_rvx(old_ptr);
  }
}

static inline void add_block_to_delayed_free_list(void *ptr)
{
  if (free_list_current_size[EXCLUSIVE_ID] == free_list_max_size[EXCLUSIVE_ID])
    increase_delayed_free_list();
  delayed_free_list[EXCLUSIVE_ID][free_list_current_size[EXCLUSIVE_ID]++] = ptr;
}

static inline void _free_really(void *ptr)
{
#if defined(USE_NPX)
  if (npx_spm_free(ptr))
    return;
#endif
  memory_block_info_t *block = (memory_block_info_t *)((uintptr_t)ptr - MEMORY_BLOCK_INFO_SIZE);
  memory_allocator_push(&(smart_allocator[EXCLUSIVE_ID]), block);
}

void _release_delayed_free_list()
{
  if (free_list_current_size[EXCLUSIVE_ID] > 0)
  {
    for (int i = 0; i < free_list_current_size[EXCLUSIVE_ID]; i++)
      _free_really(delayed_free_list[EXCLUSIVE_ID][i]);
    free_list_current_size[EXCLUSIVE_ID] = 0;
  }
}

int trackedvar_add(void *ptr, int dirty)
{
  int inserted = 0;

  // needs region check
  if (trackedvar_track_enable[EXCLUSIVE_ID])
  {
    if (is_cacheable_region(ptr))
      inserted = utset_add(&(trackedvar_track[EXCLUSIVE_ID]), (uintptr_t)ptr);
  }
  return inserted;
}

// Without checking trackedvar_track_enable[EXCLUSIVE_ID]
static inline int _trackedvar_exist(void *ptr)
{
  return utset_exist(&(trackedvar_track[EXCLUSIVE_ID]), (uintptr_t)ptr);
}

int trackedvar_exist(void *ptr)
{
  int exist = 1;
  if (trackedvar_track_enable[EXCLUSIVE_ID])
    exist = _trackedvar_exist(ptr);
  return exist;
}

void _trackedvar_flush()
{
  if (!is_sim())
    printf_function();
  // order is important: first clear the set, then flush the cache
  utset_clear(&(trackedvar_track[EXCLUSIVE_ID]));
  flush_cache();
}

int trackedvar_smart_flush(int region, ...)
{
  int is_cached = 1;
  if (trackedvar_track_enable[EXCLUSIVE_ID])
  {
    va_list args;
    va_start(args, region);

    for (int i = 0; i < region; i++)
    {
      void *ptr = va_arg(args, void *);
      assert(ptr);
      is_cached = _trackedvar_exist(ptr);
      // printf("\n0x%0x: %d", ptr, is_cached);
      if (is_cached)
        break;
    }
    va_end(args);
  }
  if (is_cached)
  {
    _release_delayed_free_list();
    _trackedvar_flush();
  }
  return is_cached;
}

void trackedvar_print()
{
  utset_print(&(trackedvar_track[EXCLUSIVE_ID]));
}

void *trackedvar_malloc(size_t size)
{
  assert(size > 0);
  size_t aligned_size = ALIGN_UP_POW2(size, CACHE_LINE_SIZE);
  size_t extended_size = aligned_size + MEMORY_BLOCK_INFO_SIZE;
  memory_block_info_t *block = NULL;

#ifdef USE_REUSE_MEMORY_ALLOCATOR
  block = memory_allocator_pop(&(smart_allocator[EXCLUSIVE_ID]), extended_size);
#endif
  if (block == NULL)
  {
    _acquire_lock_for_malloc();
    // place ptr and block in different cache lines to avoid cache-line conflicts
    heap_dram_addr += MEMORY_BLOCK_INFO_SIZE;
    heap_dram_addr = ALIGN_UP_POW2(heap_dram_addr, CACHE_LINE_SIZE);
    assert(heap_dram_size >= aligned_size);
    block = (memory_block_info_t *)(heap_dram_addr - MEMORY_BLOCK_INFO_SIZE);
    heap_dram_addr += aligned_size;
    _release_lock_for_malloc();

    block->size = extended_size;
  }

  assert(block);
  void *ptr = (void *)((uintptr_t)block + MEMORY_BLOCK_INFO_SIZE);
  assert(is_aligned_to_cacheline((uintptr_t)ptr));
  if (trackedvar_track_enable[EXCLUSIVE_ID])
    assert(!_trackedvar_exist(ptr));
  return ptr;
}

void trackedvar_free(void *ptr)
{
  assert(ptr);
#ifdef USE_REUSE_MEMORY_ALLOCATOR
  assert(is_aligned_to_cacheline((uintptr_t)ptr));
  if (trackedvar_track_enable[EXCLUSIVE_ID])
    add_block_to_delayed_free_list(ptr);
  else
    _free_really(ptr);
#endif
}

#endif