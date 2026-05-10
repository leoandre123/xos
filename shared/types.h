#pragma once

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char ubyte;

#if defined(__cplusplus)
  // bool, true, false are keywords in C++
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
  // bool, true, false are keywords in C23
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  typedef _Bool bool;
  #define true  1
  #define false 0
#else
  typedef unsigned char bool;
  #define true  1
  #define false 0
#endif