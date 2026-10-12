#ifndef __ERVP_TRAFFIC_GENERATOR_API_H__
#define __ERVP_TRAFFIC_GENERATOR_API_H__

#include "ervp_dma.h"

void traffic_generate(int index, int period_us, const dma_inst_t* inst);
void traffic_stop(int index);

#endif
