#include <stdlib.h>
#define ASSERT(x)                                                       \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s: assertion \"%s\" failed\n", __FILE_NAME__,   \
              __LINE__, __func__, #x);                                         \
      abort();                                                                 \
    }                                                                          \
  } while (0)
