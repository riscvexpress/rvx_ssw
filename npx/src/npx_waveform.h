#ifndef __NPX_WAVEFORM_H__
#define __NPX_WAVEFORM_H__

#include "npx_json.h"
#include "npx_tensor.h"

void npx_set_mel_spectorgram(const cJSON *option);
NpxTensorInfo *npx_log_mel_spectrogram(NpxTensorInfo *input, const cJSON *option);

#endif
