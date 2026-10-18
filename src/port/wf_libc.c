/*
 * The few C library functions Wolf4SDL uses that the firmware ABI does not
 * export (console only). The trigonometry only builds tables and aims a
 * few projectiles: plain series in double precision.
 */
#include <stddef.h>
#include <string.h>

#include "wf_platform.h"

#define PI 3.14159265358979323846

static double reduce(double x)
{
    const double two_pi = 2 * PI;
    long k = (long)(x / two_pi);
    x -= (double)k * two_pi;
    if (x > PI)
        x -= two_pi;
    else if (x < -PI)
        x += two_pi;
    return x;
}

double sin(double x)
{
    x = reduce(x);
    double x2 = x * x, term = x, sum = x;
    for (int n = 1; n < 18; n++) {
        term *= -x2 / (double)((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

double cos(double x)
{
    x = reduce(x);
    double x2 = x * x, term = 1.0, sum = 1.0;
    for (int n = 1; n < 18; n++) {
        term *= -x2 / (double)((2 * n - 1) * (2 * n));
        sum += term;
    }
    return sum;
}

double tan(double x)
{
    return sin(x) / cos(x);
}

/* |x| <= 0.5: the series converges quickly */
static double atan_small(double x)
{
    double x2 = x * x, term = x, sum = x;
    for (int n = 1; n < 40; n++) {
        term *= -x2;
        sum += term / (double)(2 * n + 1);
    }
    return sum;
}

double atan(double x)
{
    if (x < 0)
        return -atan(-x);
    if (x > 1.0)
        return PI / 2 - atan(1.0 / x);
    /* 0 <= x <= 1: atan(x) = atan(1/2) + atan((x - 1/2) / (1 + x/2)) */
    if (x > 0.5)
        return atan_small(0.5) + atan_small((x - 0.5) / (1.0 + 0.5 * x));
    return atan_small(x);
}

double atan2(double y, double x)
{
    if (x > 0)
        return atan(y / x);
    if (x < 0)
        return y >= 0 ? atan(y / x) + PI : atan(y / x) - PI;
    if (y > 0)
        return PI / 2;
    if (y < 0)
        return -PI / 2;
    return 0;
}

char *strerror(int errnum)
{
    (void)errnum;
    return "I/O error";
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = wf_malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}
