#include <math.h>
#include <stdint.h>

#include "ervp_printf.h"
#include "ervp_assert.h"
#include "ervp_malloc.h"
#include "ervp_stdlib.h"

#include "SPTK.h"

#include "npx_waveform.h"

void npx_set_mel_spectorgram(const cJSON *option)
{
  int sample_rate = npx_json_find_int(option, "mel_spectrogram.sample_rate", 16000);
  int n_fft = npx_json_find_int(option, "mel_spectrogram.n_fft", 512);
  int win_length = npx_json_find_int(option, "mel_spectrogram.win_length", 400);
  int n_mels = npx_json_find_int(option, "mel_spectrogram.n_mels", 40);

  init_window(HAMMING, win_length, 0);
  init_mel_filter_banks(sample_rate, n_fft, n_mels);
}

NpxTensorInfo *npx_log_mel_spectrogram(NpxTensorInfo *input, const cJSON *option)
{
  int num_samples = npx_json_find_int(option, "mel_spectrogram.num_samples", 16000);
  int sample_rate = npx_json_find_int(option, "mel_spectrogram.sample_rate", 16000);
  int n_fft = npx_json_find_int(option, "mel_spectrogram.n_fft", 512);
  int win_length = npx_json_find_int(option, "mel_spectrogram.win_length", 400);
  int hop_length = npx_json_find_int(option, "mel_spectrogram.hop_length", 160);
  int n_mels = npx_json_find_int(option, "mel_spectrogram.n_mels", 40);

  assert(npx_tensor_get_size(input, 1) == 1);
  assert(npx_tensor_get_size(input, 2) == 1);

  int waveform_length = npx_tensor_get_size(input, 0);
  int16_t *waveform = input->addr;
  float *pad_fwaveform = malloc(sizeof(float) * num_samples);

  for (int k = 0; k < num_samples; k++)
  {
    if (k < waveform_length)
      pad_fwaveform[k] = (float)(waveform[k]) / 32768;
    else
      pad_fwaveform[k] = 0;
  }

  int nframes = (int)(ceil((float)(abs(num_samples - win_length)) / hop_length));
  float *lms = malloc(sizeof(float) * nframes * n_mels);
  get_log_mel_spectrogram(pad_fwaveform, num_samples, lms, (float)sample_rate, win_length, hop_length, n_fft, n_mels);
  free(pad_fwaveform);

  NpxTensorInfo *mel_spectrogram = npx_tensor_alloc_wo_data(3);
  npx_tensor_set_size(mel_spectrogram, 0, n_mels);
  npx_tensor_set_size(mel_spectrogram, 1, nframes);
  npx_tensor_set_size(mel_spectrogram, 2, 1);
  npx_tensor_set_datatype(mel_spectrogram, MATRIX_DATATYPE_SINT16);
  npx_tensor_alloc_data(mel_spectrogram);

  int16_t *lms_int16 = mel_spectrogram->addr;
  for (int i = 0; i < nframes * n_mels; i++)
  {
    lms_int16[i] = (int16_t)lms[i];
  }
  free(lms);

  return mel_spectrogram;
}
