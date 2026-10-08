//#include <stdio.h>
// #include <ctype.h>
#include "ervp_stdlib.h"

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

double atof_rvx(const char *str)
{
  double result = 0.0;
  double fraction = 1.0;
  int sign = 1;
  int exponent = 0;
  int exp_sign = 1;

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
    result = result * 10.0 + (*str - '0');
    str++;
  }

  // Process fractional part
  if (*str == '.')
  {
    str++;
    while (isdigit(*str))
    {
      fraction *= 0.1;
      result += (*str - '0') * fraction;
      str++;
    }
  }

  // Process scientific notation (e.g., 1.23e4 or 5.67E-2)
  if (*str == 'e' || *str == 'E')
  {
    str++;
    if (*str == '-' || *str == '+')
    {
      if (*str == '-')
      {
        exp_sign = -1;
      }
      str++;
    }
    while (isdigit(*str))
    {
      exponent = exponent * 10 + (*str - '0');
      str++;
    }

    // Apply exponent
    while (exponent > 0)
    {
      if (exp_sign == 1)
      {
        result *= 10.0;
      }
      else
      {
        result *= 0.1;
      }
      exponent--;
    }
  }

  return sign * result;
}