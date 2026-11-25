#include "ervp_printf.h"
#include "npx_filename.h"

void npx_filename_init(npx_filename_t *filename, const char *app_name)
{
  filename->app_name = app_name;
  snprintf(filename->network, NPX_FILENAME_MAX, "%s.network.json", app_name);
  snprintf(filename->operator, NPX_FILENAME_MAX, "%s.operator.json", app_name);
  snprintf(filename->preprocess, NPX_FILENAME_MAX, "%s.preprocess.jsonc", app_name);
  snprintf(filename->parameter, NPX_FILENAME_MAX, "%s_parameter_quant.bin", app_name);
  npx_filename_set_sample(filename, 0);
}

void npx_filename_set_sample(npx_filename_t *filename, int sample_index)
{
  snprintf(filename->sample, NPX_FILENAME_MAX, "%s_sample_%03d.bin", filename->app_name, sample_index);
  snprintf(filename->testvector, NPX_FILENAME_MAX, "%s_layeroutput_%03d.bin", filename->app_name, sample_index);
}
