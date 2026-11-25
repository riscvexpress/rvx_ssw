//#include <stdio.h>
// #include <ctype.h>
#include "ervp_stdlib.h"
#include <stddef.h>

static inline int my_digit_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;

    return -1;
}

long strtol_rvx(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    long value = 0;
    int sign = 1;
    int digit;
    const char *start;

    /* Skip leading whitespace */
    while (isspace(*p))
        p++;

    /* Sign */
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Detect base */
    if (base == 0) {
        if (p[0] == '0') {
            if (p[1] == 'x' || p[1] == 'X') {
                base = 16;
                p += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' &&
            (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }

    start = p;

    while ((digit = my_digit_value(*p)) >= 0 &&
           digit < base) {
        value = value * base + digit;
        p++;
    }

    /* No conversion */
    if (p == start) {
        if (endptr)
            *endptr = (char *)nptr;
        return 0;
    }

    if (endptr)
        *endptr = (char *)p;

    return sign * value;
}

int atoi_rvx(const char *str)
{
  int result = 0;
  int sign = 1;

  // Skip leading whitespaces
  while (isspace(*str))
  {
    str++;
  }

  // Handle sign
  if (*str == '-' || *str == '+')
  {
    if (*str == '-')
    {
      sign = -1;
    }
    str++;
  }

  // Process integer part
  while (isdigit(*str))
  {
    result = result * 10 + (*str - '0');
    str++;
  }

  return sign * result;
}

static double pow10_rvx(int n)
{
  double result = 1.0;
  double base = 10.0;
  while (n > 0)
  {
    if (n & 1)
      result *= base;
    base *= base;
    n >>= 1;
  }
  return result;
}

double strtod_rvx(const char *nptr, char **endptr)
{
  const char *p = nptr;
  double mantissa = 0.0;
  int sign = 1;
  int exponent = 0;
  int num_digits = 0;
  int exp_sign = 1;
  int exp_value = 0;
  double result;

  while (isspace(*p))
    p++;

  if (*p == '-' || *p == '+')
  {
    if (*p == '-')
      sign = -1;
    p++;
  }

  while (isdigit(*p))
  {
    if (mantissa < 1e17)
      mantissa = mantissa * 10.0 + (*p - '0');
    else
      exponent++;
    num_digits++;
    p++;
  }

  if (*p == '.')
  {
    p++;
    while (isdigit(*p))
    {
      if (mantissa < 1e17)
      {
        mantissa = mantissa * 10.0 + (*p - '0');
        exponent--;
      }
      num_digits++;
      p++;
    }
  }

  if (num_digits == 0)
  {
    if (endptr)
      *endptr = (char *)nptr;
    return 0.0;
  }

  if (*p == 'e' || *p == 'E')
  {
    const char *q = p + 1;
    if (*q == '-' || *q == '+')
    {
      if (*q == '-')
        exp_sign = -1;
      q++;
    }
    if (isdigit(*q))
    {
      while (isdigit(*q))
      {
        if (exp_value < 10000)
          exp_value = exp_value * 10 + (*q - '0');
        q++;
      }
      exponent += exp_sign * exp_value;
      p = q;
    }
  }

  if (endptr)
    *endptr = (char *)p;

  if (exponent < 0)
  {
    if (exponent < -300)
    {
      mantissa /= pow10_rvx(300);
      exponent += 300;
    }
    result = mantissa / pow10_rvx(-exponent);
  }
  else
    result = mantissa * pow10_rvx(exponent);

  return sign * result;
}

double atof_rvx(const char *str)
{
  return strtod_rvx(str, NULL);
}
