#include "ervp_offchip_mram.h"

#include "ervp_misc_util.h"

#ifdef USE_OFFCHIP_MRAM_OPT
#define USE_BYPASS_CACHE
#endif

int offchip_mram_fill_memory_directly(ErvpMatrixInfo *result, const UNKNOWN_TYPE data)
{
  assert(result);
  int filled = 0;
#ifdef USE_BYPASS_CACHE
  int try = 0;
  if (offchip_mram_is_valid_addr(result->addr) && (!result->is_sub))
    try = 1;
  if (try)
  {
    uintptr_t addr = result->addr;
    assert(is_aligned_to_cacheline(addr));
    unsigned int size = matrix_num_bytes(result);
    if (!trackedvar_exist((void *)addr) && (size >= CACHE_LINE_SIZE))
    {
      uint32_t value = data.hex;
      switch (matrix_datatype_get_num_bits(result->datatype))
      {
      case 32:
        break;
      case 16:
        value = value * 0x0001001;
        break;
      case 8:
        value = value * 0x01010101;
        break;
      case 4:
        value = value * 0x11111111;
        break;
      case 2:
        value = value * 0x55555555;
        break;
      case 1:
        if (value != 0)
          value = -1;
        break;
      default:
        assert(0);
      }
      size = ALIGN_UP_POW2(size, 4);
      offchip_mram_msregion_set(addr, size, value);
      filled = 1;
      printf_function();
    }
  }
#endif
  return filled;
}