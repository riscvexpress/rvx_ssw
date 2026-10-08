#ifndef __ERVP_MALLOC_SYSTEM_H__
#define __ERVP_MALLOC_SYSTEM_H__

#include <stdint.h>

#include "ervp_assert.h"
#include "ervp_misc_util.h"
#include "ervp_multicore_synch.h"
#include "ervp_core_id.h"

extern volatile uintptr_t heap_sram_addr;
extern volatile uintptr_t heap_sram_size;
extern volatile uintptr_t heap_dram_addr;
extern volatile uintptr_t heap_dram_size;
extern volatile uintptr_t heap_sram_last_id;
extern volatile uintptr_t heap_dram_last_id;

static inline int is_aligned_to_cacheline(uintptr_t addr)
{
#if defined(CACHE_LINE_SIZE)
  return ((addr & (CACHE_LINE_SIZE - 1)) == 0);
#else
  return 1;
#endif
}

static inline void _prepare_heap_sram_for_multicore()
{
#ifdef INCLUDE_MULTICORE
  if (EXCLUSIVE_ID != heap_sram_last_id)
  {
    heap_sram_addr = ALIGN_UP_POW2(heap_sram_addr, CACHE_LINE_SIZE);
    heap_sram_last_id = EXCLUSIVE_ID;
  }
#endif
}

static inline uintptr_t _alloc_smallram(size_t size)
{
  uintptr_t addr = 0;
#ifdef USE_SMALL_RAM
  _prepare_heap_sram_for_multicore();
  if (size <= heap_sram_size)
  {
    addr = heap_sram_addr;
    heap_sram_addr += size;
  }
#endif
  return addr;
}

static inline void _prepare_heap_dram_for_multicore()
{
#ifdef INCLUDE_MULTICORE
  if (EXCLUSIVE_ID != heap_dram_last_id)
  {
    heap_dram_addr = ALIGN_UP_POW2(heap_dram_addr, CACHE_LINE_SIZE);
    heap_dram_last_id = EXCLUSIVE_ID;
  }
#endif
}

static inline uintptr_t _alloc_largeram(size_t size)
{
  uintptr_t addr = 0;
#ifdef USE_LARGE_RAM
  _prepare_heap_dram_for_multicore();
  if (size <= heap_dram_size)
  {
    addr = heap_dram_addr;
    heap_dram_addr += size;
  }
#endif
  return addr;
}

typedef struct
{
  uintptr_t ptr;
  unsigned int remaining_size;
} ervp_private_cacheline_t;

void *palloc_largeram_backpart(size_t size, size_t align_size);
void *palloc_cacheline(size_t size);

// permanent and private
void *ppalloc(size_t size);

#ifdef LOCK_INDEX_FOR_SYSTEM_VARIABLE
static const int MALLOC_LOCK_INDEX = LOCK_INDEX_FOR_SYSTEM_VARIABLE;
#else
static const int MALLOC_LOCK_INDEX = -1;
#endif

static inline void _acquire_lock_for_malloc()
{
  acquire_lock(MALLOC_LOCK_INDEX);
}

static inline void _release_lock_for_malloc()
{
  release_lock(MALLOC_LOCK_INDEX);
}

uintptr_t _alloc_new_memory_space(size_t size);

#endif
