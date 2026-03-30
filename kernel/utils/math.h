#pragma once

#define MAX(a, b) b > a ? b : a
#define MIN(a, b) b < a ? b : a

#define ALIGN_UP(val, aligntment) (((val) + (aligntment) - 1) / (aligntment) * (aligntment))