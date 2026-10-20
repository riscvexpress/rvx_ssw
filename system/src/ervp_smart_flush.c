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

char trackedvar_track_enable PER_CORE_DATA;

#ifdef USE_SMART_FLUSH

static utset trackedvar_track PER_CORE_DATA;
static memory_allocator_t smart_allocator PER_CORE_DATA;

static unsigned short free_list_max_size PER_CORE_DATA;
static unsigned short free_list_current_size PER_CORE_DATA;
static void **delayed_free_list PER_CORE_DATA;

static inline void increase_delayed_free_list()
{
  if (PER_CORE(free_list_max_size) == 0)
  {
    PER_CORE(free_list_max_size) = 16;
    PER_CORE(delayed_free_list) = malloc(PER_CORE(free_list_max_size) * sizeof(void *));
  }
  else
  {
    void *old_ptr = PER_CORE(delayed_free_list);
    int old_size = PER_CORE(free_list_max_size) * sizeof(void *);
    PER_CORE(free_list_max_size) = PER_CORE(free_list_max_size) << 1;
    PER_CORE(delayed_free_list) = malloc(PER_CORE(free_list_max_size) * sizeof(void *));
    memcpy_rvx(PER_CORE(delayed_free_list), old_ptr, old_size);
    free_rvx(old_ptr);
  }
}

static inline void add_block_to_delayed_free_list(void *ptr)
{
  if (PER_CORE(free_list_current_size) == PER_CORE(free_list_max_size))
    increase_delayed_free_list();
  PER_CORE(delayed_free_list)[PER_CORE(free_list_current_size)++] = ptr;
}

static inline void _free_really(void *ptr)
{
#if defined(USE_NPX)
  if (npx_spm_free(ptr))
    return;
#endif
  memory_block_info_t *block = (memory_block_info_t *)((uintptr_t)ptr - MEMORY_BLOCK_INFO_SIZE);
  memory_allocator_push(&(PER_CORE(smart_allocator)), block);
}

void _release_delayed_free_list()
{
  if (PER_CORE(free_list_current_size) > 0)
  {
    for (int i = 0; i < PER_CORE(free_list_current_size); i++)
      _free_really(PER_CORE(delayed_free_list)[i]);
    PER_CORE(free_list_current_size) = 0;
  }
}

int trackedvar_add(void *ptr, int dirty)
{
  int inserted = 0;

  // needs region check
  if (PER_CORE(trackedvar_track_enable))
  {
    if (is_cacheable_region(ptr))
      inserted = utset_add(&(PER_CORE(trackedvar_track)), (uintptr_t)ptr);
  }
  return inserted;
}

// Without checking trackedvar_track_enable[EXCLUSIVE_ID]
static inline int _trackedvar_exist(void *ptr)
{
  return utset_exist(&(PER_CORE(trackedvar_track)), (uintptr_t)ptr);
}

int trackedvar_exist(void *ptr)
{
  int exist = 1;
  if (PER_CORE(trackedvar_track_enable))
    exist = _trackedvar_exist(ptr);
  return exist;
}

void _trackedvar_flush()
{
  if (!is_sim())
    printf_function();
  // order is important: first clear the set, then flush the cache
  utset_clear(&(PER_CORE(trackedvar_track)));
  flush_cache();
}

int trackedvar_smart_flush(int region, ...)
{
  int is_cached = 1;
  if (PER_CORE(trackedvar_track_enable))
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
  utset_print(&(PER_CORE(trackedvar_track)));
}

void *trackedvar_malloc(size_t size)
{
  assert(size > 0);
  size_t aligned_size = ALIGN_UP_POW2(size, CACHE_LINE_SIZE);
  size_t extended_size = aligned_size + MEMORY_BLOCK_INFO_SIZE;
  memory_block_info_t *block = NULL;

#ifdef USE_REUSE_MEMORY_ALLOCATOR
  block = memory_allocator_pop(&(PER_CORE(smart_allocator)), extended_size);
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
  if (PER_CORE(trackedvar_track_enable))
    assert(!_trackedvar_exist(ptr));
  return ptr;
}

void trackedvar_free(void *ptr)
{
  assert(ptr);
#ifdef USE_REUSE_MEMORY_ALLOCATOR
  assert(is_aligned_to_cacheline((uintptr_t)ptr));
  if (PER_CORE(trackedvar_track_enable))
    add_block_to_delayed_free_list(ptr);
  else
    _free_really(ptr);
#endif
}

#endif