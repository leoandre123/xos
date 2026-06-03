#pragma once
#include "cdefs.h"

EXTERN_C_BEGIN

#define MAX(a, b)        ((b) > (a) ? (b) : (a))
#define MIN(a, b)        ((b) < (a) ? (b) : (a))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define ABS(x)           ((x) < 0 ? -(x) : (x))
#define SIGN(x)          ((x) > 0 ? 1 : (x) < 0 ? -1 : 0)
#define SQUARE(x)        ((x) * (x))

#define ALIGN_UP(val, alignment)                                               \
  (((val) + (alignment) - 1) / (alignment) * (alignment))

#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))

// Float constants
#define PI         3.14159265358979323846f
#define TAU        (2.0f * PI)
#define DEG_TO_RAD (PI / 180.0f)
#define RAD_TO_DEG (180.0f / PI)

// Double constants
#define PI_D         3.14159265358979323846
#define TAU_D        (2.0 * PI_D)
#define DEG_TO_RAD_D (PI_D / 180.0)
#define RAD_TO_DEG_D (180.0 / PI_D)

#define MATH_FN static inline __attribute__((unused))

// ---------------------------------------------------------------------------
// Float
// ---------------------------------------------------------------------------

MATH_FN float fabsf(float x) { return __builtin_fabsf(x); }
MATH_FN float sqrtf(float x) { return __builtin_sqrtf(x); }

MATH_FN float fmodf(float x, float y) {
  return x - (float)(long long)(x / y) * y;
}

MATH_FN float floorf(float x) {
  long long i = (long long)x;
  if (x < 0.0f && x != (float)i)
    return (float)(i - 1);
  return (float)i;
}

MATH_FN float ceilf(float x) {
  long long i = (long long)x;
  if (x > 0.0f && x != (float)i)
    return (float)(i + 1);
  return (float)i;
}

MATH_FN float roundf(float x) {
  return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

// Taylor series in [-π/2, π/2], symmetry-reduced from [-π, π]
MATH_FN float sinf(float x) {
  x = fmodf(x, TAU);
  if (x < -PI) x += TAU;
  if (x >  PI) x -= TAU;
  if (x >  PI * 0.5f) x =  PI - x;
  if (x < -PI * 0.5f) x = -PI - x;
  float x2 = x * x;
  return x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f - x2 / 5040.0f)));
}

MATH_FN float cosf(float x) { return sinf(x + PI * 0.5f); }

// Fast atan2 (~0.005 rad max error)
MATH_FN float atan2f(float y, float x) {
  if (x == 0.0f) {
    if (y > 0.0f) return  PI * 0.5f;
    if (y < 0.0f) return -PI * 0.5f;
    return 0.0f;
  }
  float r  = y / x;
  float ar = r < 0.0f ? -r : r;
  float a  = (PI / 4.0f) * r - r * (ar - 1.0f) * (0.2447f + 0.0663f * ar);
  if (x < 0.0f)
    return y >= 0.0f ? a + PI : a - PI;
  return a;
}

MATH_FN float asinf(float x) { return atan2f(x, sqrtf(1.0f - x*x)); }
MATH_FN float acosf(float x) { return atan2f(sqrtf(1.0f - x*x), x); }

// Bit-manipulation range reduction: x = m*2^e, m in [sqrt(0.5), sqrt(2)]
MATH_FN float logf(float x) {
  unsigned int u;
  __builtin_memcpy(&u, &x, sizeof u);
  int e = (int)((u >> 23) & 0xFFu) - 127;
  u = (u & 0x007FFFFFu) | 0x3F800000u;
  float m;
  __builtin_memcpy(&m, &u, sizeof m);
  if (m > 1.41421356f) { m *= 0.5f; e++; }
  float t  = (m - 1.0f) / (m + 1.0f);
  float t2 = t * t;
  float ln_m = 2.0f * t * (1.0f + t2 * (1.0f/3.0f + t2 * (1.0f/5.0f + t2 / 7.0f)));
  return ln_m + (float)e * 0.6931471805599453f;
}

MATH_FN float expf(float x) {
  float n = floorf(x * 1.4426950408889634f + 0.5f);
  float r = x - n * 0.6931471805599453f;
  float e = 1.0f + r * (1.0f + r * (0.5f + r * (1.0f/6.0f + r * (1.0f/24.0f + r * (1.0f/120.0f + r/720.0f)))));
  unsigned int u;
  __builtin_memcpy(&u, &e, sizeof u);
  u += (unsigned int)(int)n << 23;
  __builtin_memcpy(&e, &u, sizeof e);
  return e;
}

MATH_FN float powf(float b, float e) {
  if (e == 0.0f) return 1.0f;
  if (b == 0.0f) return 0.0f;
  return expf(e * logf(b));
}

// ---------------------------------------------------------------------------
// Double
// ---------------------------------------------------------------------------

MATH_FN double fabs(double x) { return __builtin_fabs(x); }
MATH_FN double sqrt(double x) { return __builtin_sqrt(x); }

MATH_FN double fmod(double x, double y) {
  return x - (double)(long long)(x / y) * y;
}

MATH_FN double floor(double x) {
  long long i = (long long)x;
  if (x < 0.0 && x != (double)i)
    return (double)(i - 1);
  return (double)i;
}

MATH_FN double ceil(double x) {
  long long i = (long long)x;
  if (x > 0.0 && x != (double)i)
    return (double)(i + 1);
  return (double)i;
}

MATH_FN double round(double x) {
  return x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5);
}

// Taylor series in [-π/2, π/2], symmetry-reduced — one extra term vs float
MATH_FN double sin(double x) {
  x = fmod(x, TAU_D);
  if (x < -PI_D) x += TAU_D;
  if (x >  PI_D) x -= TAU_D;
  if (x >  PI_D * 0.5) x =  PI_D - x;
  if (x < -PI_D * 0.5) x = -PI_D - x;
  double x2 = x * x;
  return x * (1.0 - x2 * (1.0/6.0 - x2 * (1.0/120.0 - x2 * (1.0/5040.0 - x2 / 362880.0))));
}

MATH_FN double cos(double x) { return sin(x + PI_D * 0.5); }

// Fast atan2 (~0.005 rad max error)
MATH_FN double atan2(double y, double x) {
  if (x == 0.0) {
    if (y > 0.0) return  PI_D * 0.5;
    if (y < 0.0) return -PI_D * 0.5;
    return 0.0;
  }
  double r  = y / x;
  double ar = r < 0.0 ? -r : r;
  double a  = (PI_D / 4.0) * r - r * (ar - 1.0) * (0.2447 + 0.0663 * ar);
  if (x < 0.0)
    return y >= 0.0 ? a + PI_D : a - PI_D;
  return a;
}

MATH_FN double asin(double x) { return atan2(x, sqrt(1.0 - x*x)); }
MATH_FN double acos(double x) { return atan2(sqrt(1.0 - x*x), x); }

MATH_FN double log(double x) {
  unsigned long long u;
  __builtin_memcpy(&u, &x, sizeof u);
  int e = (int)((u >> 52) & 0x7FFull) - 1023;
  u = (u & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
  double m;
  __builtin_memcpy(&m, &u, sizeof m);
  if (m > 1.41421356237309504) { m *= 0.5; e++; }
  double t  = (m - 1.0) / (m + 1.0);
  double t2 = t * t;
  double ln_m = 2.0 * t * (1.0 + t2 * (1.0/3.0 + t2 * (1.0/5.0 + t2 * (1.0/7.0 + t2 / 9.0))));
  return ln_m + (double)e * 0.6931471805599453;
}

MATH_FN double exp(double x) {
  double n = floor(x * 1.4426950408889634 + 0.5);
  double r = x - n * 0.6931471805599453;
  double e = 1.0 + r * (1.0 + r * (0.5 + r * (1.0/6.0 + r * (1.0/24.0 + r * (1.0/120.0 + r * (1.0/720.0 + r/5040.0))))));
  unsigned long long u;
  __builtin_memcpy(&u, &e, sizeof u);
  u += (unsigned long long)(long long)n << 52;
  __builtin_memcpy(&e, &u, sizeof e);
  return e;
}

MATH_FN double pow(double b, double e) {
  if (e == 0.0) return 1.0;
  if (b == 0.0) return 0.0;
  return exp(e * log(b));
}

EXTERN_C_END
