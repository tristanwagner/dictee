#ifndef _BUFFER_H_
#define _BUFFER_H_
#include <stdlib.h>
#include <string.h>

// append buffer used to construct a string to pass to write
typedef struct {
  char *b;
  int len;
} buffer;

#define BUFFER_INIT                                                              \
  { NULL, 0 }

void buffer_append(buffer *ab, const char *s, int len);
void free_buffer(buffer *ab);

#endif
