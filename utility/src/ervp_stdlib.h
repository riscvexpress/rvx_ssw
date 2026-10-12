#ifndef __ERVP_STDLIB_H__
#define __ERVP_STDLIB_H__

#define abs abs_rvx
static inline int abs_rvx(int x)
{
  return (x < 0) ? -x : x;
}

#define isdigit isdigit_rvx
static inline int isdigit_rvx(int c)
{
  return (c >= '0' && c <= '9');
}

#define isspace isspace_rvx
static inline int isspace_rvx(int c)
{
  return (c == ' ') ||
         (c == '\t') ||
         (c == '\n') ||
         (c == '\v') ||
         (c == '\f') ||
         (c == '\r');
}

#define strtol strtol_rvx
long strtol_rvx(const char *nptr, char **endptr, int base);

#define atoi atoi_rvx
int atoi_rvx(const char *str);

#define atof atof_rvx
double atof_rvx(const char *str);

#endif
