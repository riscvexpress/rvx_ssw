#ifndef __NPX_FILENAME_H__
#define __NPX_FILENAME_H__

#define NPX_FILENAME_MAX 128

typedef struct
{
  const char *app_name;
  char network[NPX_FILENAME_MAX];
  char operator[NPX_FILENAME_MAX];
  char preprocess[NPX_FILENAME_MAX];
  char parameter[NPX_FILENAME_MAX];
  char sample[NPX_FILENAME_MAX];
  char testvector[NPX_FILENAME_MAX];
} npx_filename_t;

void npx_filename_init(npx_filename_t *filename, const char *app_name);
void npx_filename_set_sample(npx_filename_t *filename, int sample_index);

#endif
