#include "ervp_traffic_geneator_api.h"
#include "platform_info.h"

#ifdef INCLUDE_TRAFFIC_GENERATOR

#include "ip_instance_info.h"

static ervp_dma_hwinfo_t get_traffic_hwinfo(int index)
{
  ervp_dma_hwinfo_t hwinfo = *i_traffic_generator00_control_info;
  hwinfo.baseaddr += (I_TRAFFIC_GENERATOR00_CONTROL_SIZE*index);
  return hwinfo;
}

void traffic_stop(int index)
{
  if(index < NUM_TRAFFIC_GENERATOR)
  {
    ervp_dma_hwinfo_t hwinfo = get_traffic_hwinfo(index);
    dma_clear(&hwinfo);
  }
}

void traffic_generate(int index, int period_us, const dma_inst_t* inst)
{
  if(index < NUM_TRAFFIC_GENERATOR)
  {
    ervp_dma_hwinfo_t hwinfo = get_traffic_hwinfo(index);
    dma_inst_t periodic_inst = *inst;
    periodic_inst.period_us = period_us;
    periodic_inst.periodic = 1;
    dma_clear(&hwinfo);
    dma_inst_push(&hwinfo, &periodic_inst, 0);
  }
}

#else

void traffic_stop(int index){}
void traffic_generate(int index, int period_us, const dma_inst_t* inst){}

#endif
