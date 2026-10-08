#include "ervp_malloc_system.h"
#include "ervp_variable_allocation.h"

volatile uintptr_t heap_sram_addr NOTCACHED_DATA = 0;
volatile uintptr_t heap_sram_size NOTCACHED_DATA = 0;
volatile uintptr_t heap_dram_addr NOTCACHED_DATA = 0;
volatile uintptr_t heap_dram_size NOTCACHED_DATA = 0;
volatile uintptr_t heap_sram_last_id NOTCACHED_DATA = -1;
volatile uintptr_t heap_dram_last_id NOTCACHED_DATA = -1;

void *palloc_largeram_backpart(size_t size, size_t align_size)
{
#ifndef USE_LARGE_RAM
  assert(0);
#endif
  assert(size > 0);
  uintptr_t ptr;

  _acquire_lock_for_malloc();
  ptr = heap_dram_addr - size + heap_dram_size;
  ptr = ALIGN_DOWN_POW2(ptr, align_size);
#if defined(CACHE_LINE_SIZE)
  ptr = ALIGN_DOWN_POW2(ptr, CACHE_LINE_SIZE);
#endif
  heap_dram_size = ptr - heap_dram_addr;
  assert(heap_dram_size >= 0);
  _release_lock_for_malloc();

  return (void *)ptr;
}

static ervp_private_cacheline_t private_cacheline[NUM_CORE] DATA_BSS;

#if defined(CACHE_LINE_SIZE)
static const int ALIGN_SIZE = CACHE_LINE_SIZE;
#else
static const int ALIGN_SIZE = 16;
#endif

static uintptr_t _alloc_cacheline(size_t aligned_size)
{
  assert(is_aligned_to_cacheline((uintptr_t)aligned_size));
  assert(aligned_size > 0);
  uintptr_t addr;

  _acquire_lock_for_malloc();
#if defined(USE_LARGE_RAM)
#if defined(CACHE_LINE_SIZE)
  heap_dram_addr = ALIGN_UP_POW2(heap_dram_addr, CACHE_LINE_SIZE);
#endif
  addr = heap_dram_addr;
  heap_dram_addr += aligned_size;
#elif defined(USE_SMALL_RAM)
#if defined(CACHE_LINE_SIZE)
  heap_sram_addr = ALIGN_UP_POW2(heap_sram_addr, CACHE_LINE_SIZE);
#endif
  addr = heap_sram_addr;
  heap_sram_addr += aligned_size;
#endif
  _release_lock_for_malloc();
  assert(addr);
  return addr;
}

void *ppalloc(size_t size)
{
  assert(size > 0);
  ervp_private_cacheline_t *cacheline = &(private_cacheline[EXCLUSIVE_ID]);
  if (cacheline->remaining_size < size)
  {
    size_t aligned_size = ALIGN_UP_POW2(size, ALIGN_SIZE);
    uintptr_t ptr = _alloc_cacheline(aligned_size);
    if (ptr == (cacheline->ptr + cacheline->remaining_size))
      cacheline->remaining_size += aligned_size;
    else
    {
      cacheline->ptr = ptr;
      cacheline->remaining_size = aligned_size;
    }
  }
  uintptr_t ptr = cacheline->ptr;
  cacheline->ptr += size;
  cacheline->remaining_size -= size;
  assert(cacheline->remaining_size >= 0);
  return (void *)ptr;
}

uintptr_t _alloc_new_memory_space(size_t size)
{
  uintptr_t addr = 0;
  _acquire_lock_for_malloc();
#if defined(USE_LARGE_RAM)
  addr = _alloc_largeram(size);
#elif defined(USE_SMALL_RAM)
  addr = _alloc_smallram(size);
#endif
  _release_lock_for_malloc();
  return addr;
}