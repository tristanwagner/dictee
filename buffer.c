#include "buffer.h"

void buffer_append(buffer *ab, const char *s, int len) {
  char *buf = realloc(ab->b, ab->len + len);

  if (buf == NULL)
    return;
  memcpy(&buf[ab->len], s, len);
  ab->b = buf;
  ab->len += len;
}

void free_buffer(buffer *ab) { free(ab->b); }
