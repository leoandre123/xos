#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      panic_assert(__FILE__, __LINE__, #expr);                                 \
    }                                                                          \
  } while (0)