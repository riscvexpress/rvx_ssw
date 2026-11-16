#include "ervp_extinput_backend.h"
#include "ervp_assert.h"

static inline int extinput_backend_config_index(const ervp_extinput_hwinfo_t *const hwinfo)
{
  return hwinfo->size_of_config - sizeof(ervp_extinput_config_t);
}

static inline ervp_extinput_config_t *extinput_backend_config(const ervp_extinput_hwinfo_t *const hwinfo, mmio_data_t *buffer)
{
  return (ervp_extinput_config_t *)(((unsigned char *)buffer) + extinput_backend_config_index(hwinfo));
}

void extinput_set_active(const ervp_extinput_hwinfo_t *const hwinfo, int active)
{
  mmio_data_t buffer[rshift_ru(hwinfo->size_of_config, 2)];
  mmiox1_config_read(hwinfo, buffer);
  extinput_backend_config(hwinfo, buffer)->active = (active != 0);
  mmiox1_config_write(hwinfo, buffer);
}

int extinput_is_active(const ervp_extinput_hwinfo_t *const hwinfo)
{
  mmio_data_t buffer[rshift_ru(hwinfo->size_of_config, 2)];
  mmiox1_config_read(hwinfo, buffer);
  return extinput_backend_config(hwinfo, buffer)->active;
}
