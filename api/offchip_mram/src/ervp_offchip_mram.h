#ifndef __ERVP_OFFCHIP_MRAM_H__
#define __ERVP_OFFCHIP_MRAM_H__

#include "ervp_assert.h"
#include "ervp_printf.h"
#include "ervp_mmiox1.h"
#include "ervp_matrix.h"
#include "ervp_offchip_mram_memorymap_offset.h"

#include "ip_instance_info.h"

typedef struct
{
  unsigned int cycle_onehot : 16;
} offchip_mram_config_t;

typedef struct
{
  uint32_t opcode;
  uint32_t operand;
} offchip_mram_inst_t;

static inline int offchip_mram_is_valid_addr(uint32_t addr)
{
  int valid = 0;
  if ((addr >= I_SYSTEM_OFFCHIP_MRAM_BASEADDR) && (addr <= I_SYSTEM_OFFCHIP_MRAM_LASTADDR))
    valid = 1;
  return valid;
}

static inline void offchip_mram_set_access_cycle(int access_cycle)
{
  offchip_mram_config_t offchip_mram_config;
  mmiox1_config_read(i_system_offchip_mram_control_info, &offchip_mram_config);
  offchip_mram_config.cycle_onehot = 1 << access_cycle;
  mmiox1_config_write(i_system_offchip_mram_control_info, &offchip_mram_config);
}

static inline offchip_mram_config_t offchip_mram_config_read()
{
  offchip_mram_config_t offchip_mram_config;
  mmiox1_config_read(i_system_offchip_mram_control_info, &offchip_mram_config);
  return offchip_mram_config;
}

static inline int offchip_mram_get_access_cycle()
{
  unsigned int cycle_onehot = offchip_mram_config_read().cycle_onehot;
  assert(cycle_onehot != 0);
  int access_cycle = 0;
  while (1)
  {
    if ((cycle_onehot & 1) == 1)
      break;
    access_cycle++;
    cycle_onehot >>= 1;
  }
  if (access_cycle == 15)
    access_cycle = 0;
  return access_cycle;
}

static inline void offchip_mram_profile_start(int counter_index, int property)
{
  offchip_mram_inst_t offchip_mram_inst;
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_PROFILE_START;
  offchip_mram_inst.opcode |= property;
  if (counter_index == (-1))
    offchip_mram_inst.operand = counter_index;
  else
    offchip_mram_inst.operand = 1 << counter_index;

  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

static inline void offchip_mram_profile_finish(int counter_index)
{
  offchip_mram_inst_t offchip_mram_inst;
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_PROFILE_FINISH;
  if (counter_index == (-1))
    offchip_mram_inst.operand = counter_index;
  else
    offchip_mram_inst.operand = 1 << counter_index;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

static inline void offchip_mram_profile_print()
{
  printf_must("\n\n[RVX/MRAM] profile\n");
  offchip_mram_inst_t offchip_mram_inst;
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_PROFILE_OUTPUT;
  for (int i = 0; i < NUM_OFFCHIP_MRAM_PROFILER; i++)
  {
    uint64_t write_count;
    offchip_mram_inst.operand = 1 << i;
    mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
    mmiox1_output_pop(i_system_offchip_mram_control_info, &write_count, 1);
    printf_must("%2d: %d\n", i, write_count);
  }
}

static inline void offchip_mram_unregion_set(uint32_t start_addr, size_t num_bytes)
{
  assert(offchip_mram_is_valid_addr(start_addr));
  if (num_bytes == 0)
    return;

  offchip_mram_inst_t offchip_mram_inst;
  //
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_UNREGION_START_ADDR;
  offchip_mram_inst.operand = start_addr;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
  //
  uint32_t last_addr = start_addr + (num_bytes - 1);
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_UNREGION_LART_ADDR;
  offchip_mram_inst.operand = last_addr;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
  //
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_UNREGION_ACTIVE;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

static inline void offchip_mram_unregion_all_clear()
{
  offchip_mram_inst_t offchip_mram_inst;
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_UNREGION_ALL_CLEAR;
  offchip_mram_inst.operand = 0;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

static inline void offchip_mram_msregion_set(uint32_t start_addr, size_t num_bytes, int32_t value)
{
  assert(offchip_mram_is_valid_addr(start_addr));
  assert((start_addr & 3) == 0);
  assert((num_bytes & 3) == 0);

  if (num_bytes == 0)
    return;

  offchip_mram_inst_t offchip_mram_inst;
  // MUST set start_addr first
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_MSREGION_START_ADDR;
  offchip_mram_inst.operand = start_addr;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
  //
  uint32_t last_addr = start_addr + (num_bytes - 1);
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_MSREGION_LART_ADDR;
  offchip_mram_inst.operand = last_addr;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
  //
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_MSREGION_ACTIVE;
  offchip_mram_inst.operand = value;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

static inline void offchip_mram_msregion_all_clear()
{
  offchip_mram_inst_t offchip_mram_inst;
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_MSREGION_ALL_CLEAR;
  offchip_mram_inst.operand = 0;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

static inline void offchip_mram_msregion_flush()
{
  offchip_mram_inst_t offchip_mram_inst;
  offchip_mram_inst.opcode = OFFCHIP_MRAM_MMIOX_INST_OPCODE_MSREGION_FLUSH;
  offchip_mram_inst.operand = 0;
  mmiox1_inst_push(i_system_offchip_mram_control_info, &offchip_mram_inst, 1, 0);
}

// Returns true on success, false otherwise.
int offchip_mram_fill_memory_directly(ErvpMatrixInfo *result, const UNKNOWN_TYPE data);

#endif
