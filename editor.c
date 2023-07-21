#include "editor.h"
#include <stdlib.h>
#include <string.h>

static editor_config ec = {0};

// save cursor pos
static editor_cursor_position ecp = {0};

char *C_HL_extensions[] = {".c", ".h", ".cpp", ".hpp", NULL};
char *C_HL_keywords[] = {
    "switch",  "if",    "while",    "for",     "break",   "continue",
    "return",  "else",  "struct",   "union",   "typedef", "static",
    "enum",    "class", "#include", "#define", "#endif",  "#if",
    "#else",   "#elif", "#idef",    "#ifndef", "case",    "default",
    "int|",    "long|", "double|",  "float|",  "char|",   "unsigned|",
    "signed|", "void|", NULL};

char *JS_HL_extensions[] = {".js", ".jsx", ".cjs", ".mjs", NULL};
char *JS_HL_keywords[] = {"switch",   "if",      "while",   "for",     "break",
                          "continue", "return",  "default", "import",  "export",
                          "require",  "console", "else",    "static",  "const|",
                          "class",    "case",    "let|",    "String|", "Array|",
                          "Set|",     "Buffer|", NULL};

editor_syntax HLDB[] = {
    {"c", C_HL_extensions, "//", "/*", "*/", C_HL_keywords,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_COMMENT},
    {"js", JS_HL_extensions, "//", "/*", "*/", JS_HL_keywords,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_COMMENT},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

void editor_save_cursor_position() {
  ecp.cx = ec.cx;
  ecp.cy = ec.cy;
  ecp.colOffset = ec.colOffset;
  ecp.rowOffset = ec.rowOffset;
}

void editor_restore_cursor_position() {
  ec.cx = ecp.cx;
  ec.cy = ecp.cy;
  ec.colOffset = ecp.colOffset;
  ec.rowOffset = ecp.rowOffset;
}

char *editor_prompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editor_set_status_msg(prompt, buf);
    editor_refresh_screen();

    int c = editor_read_key();
    if (c == ESC) {
      editor_set_status_msg("");
      if (callback)
        callback(NULL, c);
      free(buf);
      return NULL;
    } else if (c == BACKSPACE || c == DEL_KEY || c == CTRL_KEY('h')) {
      if (buflen > 0) {
        buf[--buflen] = '\0';
      }
    } else if ((!iscntrl(c) && c < 128) || c == '\r') {
      if (c == '\r') {
        editor_set_status_msg("");
        if (callback)
          callback(buf, c);
        return buf;
      }
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback)
      callback(buf, c);
  }
}

int editor_confirm() {
  char *response = editor_prompt("Are you sure ? [Y/n] %s", NULL);
  int result = 0;
  if (response == NULL) {
    return result;
  } else if (response[0] == '\0' || response[0] == 'y' || response[0] == 'Y') {
    result = 1;
  }

  free(response);
  return result;
}

void editor_search_prompt_callback(char *query, int c) {
  // -1 if no match or index of last match
  static int last_match = -1;
  // search direction  1 forward | -1 backward
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl != NULL) {
    memcpy(ec.row[saved_hl_line].hl, saved_hl, ec.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  switch (c) {
  case '\r':
  case ESC:
    last_match = -1;
    direction = 1;
    return;
    break;
  case MOVE_CURSOR_DOWN:
  case MOVE_CURSOR_RIGHT:
    direction = 1;
    break;
  case MOVE_CURSOR_UP:
  case MOVE_CURSOR_LEFT:
    direction = -1;
    break;
  default:
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;

  int current = last_match;
  // search
  // TODO: handle multiple results
  for (int i = 0; i < ec.numRows; i++) {
    current += direction;
    if (current == -1)
      current = ec.numRows - 1;
    else if (current == ec.numRows)
      current = 0;

    editor_row *row = &ec.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = ec.cy = current;
      int rx = match - row->render;
      ec.cx = editor_row_rx_to_cx(row, rx);
      // scroll bottom so result will be top of screen
      ec.rowOffset = ec.numRows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);

      memset(&row->hl[rx], HL_SEARCH_RESULT, str_len(query));
      break;
    }
  }
}
void editor_find() {
  editor_save_cursor_position();
  // TODO:
  // enter search mode
  // make binds to navigate results
  char *query = editor_prompt("Search: %s (ESC to cancel)",
                              editor_search_prompt_callback);
  if (query) {
    free(query);
  } else {
    editor_restore_cursor_position();
  }
}

int editor_save_file(const char *filename, char *buffer, long len) {
  int fd = open(filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buffer, len) == len) {
        close(fd);
        editor_set_status_msg("%d bytes writen to \"%s\"", len, filename);
        return len;
      }
    }
    close(fd);
  }

  editor_set_status_msg("I/O error while saving using editor_save_file(): %s",
                        strerror(errno));
  return 0;
}

void editor_open() {
  char *query = editor_prompt("Open file: %s (ESC to cancel)", NULL);
  if (query) {
    editor_open_file(query);
    free(query);
  }
}
void editor_save() {
  if (ec.filename == NULL) {
    ec.filename = editor_prompt("Save as: %s (ESC to cancel)", NULL);
    if (ec.filename == NULL || editor_confirm() != 1) {
      ec.filename = NULL;
      editor_set_status_msg("Save aborted");
      return;
    }
    editor_select_filetype_syntax();
  }
  size_t len;
  char *buf = editor_rows_to_string(&len);

  if (editor_save_file(ec.filename, buf, (ssize_t)len) > 0) {
    ec.dirty = 0;
  }

  free(buf);
}

char *editor_rows_to_string(size_t *buflen) {
  size_t len = 0;
  int j;

  for (j = 0; j < ec.numRows; j++) {
    len += ec.row[j].size + 1;
  }

  *buflen = len;

  char *buf = malloc(len);
  char *p = buf;

  for (j = 0; j < ec.numRows; j++) {
    memcpy(p, ec.row[j].chars, ec.row[j].size);
    p += ec.row[j].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editor_insert_newline() {
  if (ec.cx == 0) {
    editor_insert_row(ec.cy, "", 0);
  } else {
    editor_row *row = &ec.row[ec.cy];
    editor_insert_row(ec.cy + 1, &row->chars[ec.cx], row->size - ec.cx);
    row = &ec.row[ec.cy];
    row->size = ec.cx;
    row->chars[row->size] = '\0';
    editor_update_row(row);
  }
  ec.cy++;
  ec.cx = 0;
}

void editor_row_insert_char(editor_row *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editor_update_row(row);
  ec.dirty++;
}

void editor_insert_char(int c) {
  if (ec.cy == ec.numRows) {
    editor_insert_row(ec.numRows, "", 0);
  }
  int len = ec.row[ec.cy].rsize;

  editor_row_insert_char(&ec.row[ec.cy], ec.cx, c);
  int offset = ec.row[ec.cy].rsize - len;
  ec.cx += offset >= 0 ? offset : 0;
}

void editor_row_delete_char(editor_row *row, int at) {
  if (row == NULL || at < 0 || at > row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->chars[row->size] = '\0';
  editor_set_status_msg("ec.cx: %d | rsize %d | at %d", ec.cx,
                        ec.row[ec.cy].size, at);
  row->size--;
  editor_update_row(row);
  ec.dirty++;
}

void editor_delete_char() {
  if (ec.cx == 0 && ec.cy == 0)
    return;

  editor_row *row = &ec.row[ec.cy];
  if (ec.cx > 0) {
    editor_row_delete_char(row, ec.cx - 1);
    ec.cx--;
  } else {
    if (ec.cy >= ec.numRows) {
      ec.cy = ec.numRows - 1;
      editor_delete_row(ec.cy);
      return;
    }
    ec.cx = ec.row[ec.cy - 1].size;
    editor_row_append_string(&ec.row[ec.cy - 1], row->chars, row->size);
    editor_delete_row(ec.cy);
    ec.cy--;
  }
}

// cursor x to row x => support tab display in editor
int editor_row_cx_to_rx(editor_row *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TAB_SIZE - 1) - (rx % TAB_SIZE);
    rx++;
  }

  return rx;
}

int editor_row_rx_to_cx(editor_row *row, int rx) {
  int currentrx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      currentrx += (TAB_SIZE - 1) - (currentrx % TAB_SIZE);
    currentrx++;

    if (currentrx > rx)
      return cx;
  }

  return cx;
}

void editor_row_update_syntax(editor_row *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_DEFAULT, row->rsize);

  if (ec.syntax == NULL)
    return;

  char **keywords = ec.syntax->keywords;

  char *slc_start = ec.syntax->single_line_comment_start;
  char *mlc_start = ec.syntax->multiline_comment_start;
  char *mlc_end = ec.syntax->multiline_comment_end;

  int slc_len = slc_start ? str_len(slc_start) : 0;
  int mlcs_len = mlcs_len ? str_len(mlc_start) : 0;
  int mlce_len = mlce_len ? str_len(mlc_end) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->index > 0 && ec.row[row->index - 1].hl_open_comment);

  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = i > 0 ? row->hl[i - 1] : HL_DEFAULT;

    if (ec.syntax->flags & HL_HIGHLIGHT_COMMENT) {
      // handle single line comment
      if (slc_len && !in_string && !in_comment) {
        if (!strncmp(&row->render[i], slc_start, slc_len)) {
          memset(&row->hl[i], HL_COMMENT, row->rsize - i);
          break;
        }
      }

      // handle multi line comment
      if (mlcs_len && mlce_len && !in_string) {
        if (in_comment) {
          row->hl[i] = HL_MLCOMMENT;
          if (!strncmp(&row->render[i], mlc_end, mlce_len)) {
            memset(&row->hl[i], HL_MLCOMMENT, mlce_len);
            i += mlce_len;
            in_comment = 0;
            prev_sep = 1;
            continue;
          } else {
            i++;
            continue;
          }
        } else if (!strncmp(&row->render[i], mlc_start, mlcs_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mlcs_len);
          i += mlcs_len;
          in_comment = 1;
          continue;
        }
      }
    }

    // handle strings
    if (ec.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string)
          in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else if (c == '"' || c == '\'') {
        in_string = c;
        row->hl[i] = HL_STRING;
        i++;
        continue;
      }
    }

    // handle numbers
    if (ec.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if (isdigit(c) && (prev_sep || prev_hl == HL_NUMBER) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    // handle keywords
    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int len = str_len(keywords[j]);
        // kw2 are the ones that ends with |
        int kw2 = keywords[j][len - 1] == '|';
        if (kw2)
          len--;
        if (!strncmp(&row->render[i], keywords[j], len) &&
            c_is_separator(row->render[i + len])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, len);
          i += len;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }
    prev_sep = c_is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;

  if (changed && row->index + 1 < ec.numRows)
    editor_row_update_syntax(&ec.row[row->index + 1]);
}

void editor_update_syntax() {
  for (int i = 0; i < ec.numRows; i++) {
    editor_row_update_syntax(&ec.row[i]);
  }
}

// TODO:
// define these in a config so we can do theming
int editor_syntax_to_color(int hl) {
  switch (hl) {
  case HL_NUMBER:
    return 31;
  case HL_STRING:
    return 35;
  case HL_SEARCH_RESULT:
    return 93;
  case HL_MLCOMMENT:
  case HL_COMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_DEFAULT:
    return 39;
  default:
    return 37;
  }
}

void editor_select_filetype_syntax() {
  ec.syntax = NULL;
  if (ec.filename == NULL)
    return;

  char *ext = strstr(ec.filename, ".");

  for (unsigned int i = 0; i < HLDB_ENTRIES; i++) {
    editor_syntax *s = &HLDB[i];
    unsigned int j = 0;
    while (s->filematches[j]) {
      int is_ext = s->filematches[j][0] == '.';
      if ((is_ext && ext && !strcmp(ext, s->filematches[j])) ||
          (!is_ext && strstr(ec.filename, s->filematches[j]))) {
        ec.syntax = s;
        editor_update_syntax();
        return;
      }
      j++;
    }
  }
}

void editor_update_row(editor_row *row) {
  int j, tabs = 0;

  // suport tabs
  // TODO: add TAB_SIZE as space if tab pressed
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  if (row->render != NULL)
    free(row->render);

  row->render = malloc(row->size + tabs * (TAB_SIZE - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TAB_SIZE != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  row->rsize = idx;

  editor_row_update_syntax(row);
}

void editor_insert_row(int at, char *line, int linelen) {
  if (at < 0 || at > ec.numRows)
    return;
  // realloc a new row
  ec.row = realloc(ec.row, sizeof(editor_row) * (ec.numRows + 1));
  memmove(&ec.row[at + 1], &ec.row[at], sizeof(editor_row) * (ec.numRows - at));

  // increment next row indexes
  for (int i = at + 1; i < ec.numRows; i++)
    ec.row[i].index++;

  ec.row[at].index = at;
  ec.row[at].size = linelen;
  ec.row[at].chars = malloc(linelen + 1);

  memcpy(ec.row[at].chars, line, linelen);
  ec.row[at].chars[linelen] = '\0';
  ec.row[at].rsize = 0;
  ec.row[at].render = NULL;
  ec.row[at].hl = NULL;
  ec.row[at].hl_open_comment = 0;

  editor_update_row(&ec.row[at]);
  ec.numRows++;
  ec.dirty++;
}

void editor_row_append_string(editor_row *row, char *s, size_t len) {
  if (len < 1 || row == NULL || s == NULL) {
    return;
  }
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editor_update_row(row);
  ec.dirty++;
}

void editor_free_row(editor_row *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editor_delete_row(int at) {
  if (at < 0 || at >= ec.numRows)
    return;
  editor_free_row(&ec.row[at]);
  memmove(&ec.row[at], &ec.row[at + 1],
          sizeof(editor_row) * (ec.numRows - at - 1));
  // decrement next row indexes
  for (int i = at; i < ec.numRows; i++)
    ec.row[i].index--;
  ec.numRows--;
  ec.dirty++;
  if (ec.filename == NULL && ec.numRows == 0) {
    ec.dirty = 0;
  }
}

void editor_open_file(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    editor_set_status_msg("Could not open file \"%s\"", filename);
    return;
  } else {
    editor_free_current_buffer();
  }

  ec.filename = strdup(filename);
  editor_select_filetype_syntax();

  char *line = NULL;
  size_t linecap = 0;
  int linelen;

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {

      linelen--;
    }
    editor_insert_row(ec.numRows, line, linelen);
  }

  if (ec.numRows == 0) {
    editor_insert_row(0, "", 0);
  }

  free(line);
  fclose(fp);

  ec.dirty = 0;
}

void editor_draw_rows(buffer *ab) {
  int y;
  for (y = 0; y < ec.screenRows; y++) {
    int fileRow = y + ec.rowOffset;
    if (fileRow >= ec.numRows) {
      if (ec.numRows == 0 && y == ec.screenRows / 3) {
        char message[64];
        int messageLen =
            snprintf(message, sizeof(message), "Dictée - version %s", VERSION);
        if (messageLen > ec.screenCols)
          messageLen = ec.screenCols;
        int padding = (ec.screenCols - messageLen) / 2;
        if (padding) {
          buffer_append(ab, "~", 1);
          padding--;
        }
        while (padding--)
          buffer_append(ab, " ", 1);
        buffer_append(ab, message, messageLen);
      } else {
        buffer_append(ab, "~", 1);
      }
    } else {
      int len = ec.row[fileRow].rsize - ec.colOffset;
      len = len < 0 ? 0 : len;
      len = len > ec.screenCols ? ec.screenCols : len;
      char *c = &ec.row[fileRow].render[ec.colOffset];
      unsigned char *hl = &ec.row[fileRow].hl[ec.colOffset];
      int current_color = -1;
      for (int i = 0; i < len; i++) {
        int color = editor_syntax_to_color(hl[i]);
        if (iscntrl(c[i])) {
          char sym = (c[i] <= 26 ? '@' + c[i] : '?');
          buffer_append(ab, "\x1b[7m", 4);
          buffer_append(ab, &sym, 1);
          buffer_append(ab, "\x1b[m", 3);
          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            buffer_append(ab, buf, clen);
          }
        } else {
          if (current_color != color) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            buffer_append(ab, buf, clen);
          }
          current_color = color;
          buffer_append(ab, &c[i], 1);
        }
      }
      // reset to default color at end of line
      // to prevent last line from coloring all
      // the rest of the term ?
      // Maybe I should just specify the color
      // directly in draw_status_bar etc
      buffer_append(ab, "\x1b[m", 3);
    }

    // clear from cursor to end of line
    buffer_append(ab, "\x1b[K", 3);
    // clear line
    buffer_append(ab, "\r\n", 2);
  }
}

void editor_draw_status_bar(buffer *ab) {
  buffer_append(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     ec.filename ? ec.filename : "[No Name]", ec.numRows,
                     ec.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | [%d/%d] %d/%d",
                      ec.syntax ? ec.syntax->filetype : "no ft", ec.cx, ec.cy,
                      ec.cy + 1, ec.numRows);
  if (len > ec.screenCols)
    len = ec.screenCols;
  buffer_append(ab, status, len);
  while (len < ec.screenCols) {
    if (ec.screenCols - len == rlen) {
      buffer_append(ab, rstatus, rlen);
      break;
    } else {
      buffer_append(ab, " ", 1);
      len++;
    }
  }
  buffer_append(ab, "\x1b[m", 3);
  buffer_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(buffer *ab) {
  buffer_append(ab, "\x1b[K", 3);
  int msglen = strlen(ec.statusmsg);
  if (msglen > ec.screenCols)
    strlen(ec.statusmsg);
  if (msglen && time(NULL) - ec.statusmsg_time < 5)
    buffer_append(ab, ec.statusmsg, msglen);
}

void editor_set_status_msg(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ec.statusmsg, sizeof(ec.statusmsg), fmt, ap);
  va_end(ap);
  ec.statusmsg_time = time(NULL);
}

void editor_scroll() {
  ec.rx = 0;

  if (ec.cy < ec.numRows) {
    ec.rx = editor_row_cx_to_rx(&ec.row[ec.cy], ec.cx);
  }

  // scroll back
  if (ec.cy < ec.rowOffset) {
    ec.rowOffset = ec.cy;
  }

  // scroll down
  if (ec.cy >= ec.rowOffset + ec.screenRows) {
    ec.rowOffset = ec.cy - ec.screenRows + 1;
  }

  if (ec.rx < ec.colOffset) {
    ec.colOffset = ec.rx;
  }

  if (ec.rx >= ec.colOffset + ec.screenCols) {
    ec.colOffset = ec.rx - ec.screenCols + 1;
  }
}

void editor_refresh_screen() {
  editor_scroll();
  buffer ab = BUFFER_INIT;

  // https://vt100.net/docs/vt100-ug/chapter3.html
  // for details on escape sequence
  // hide cursor to prevent flickering
  buffer_append(&ab, "\x1b[?25l", 6);
  // this one basically means clear the entire screen
  // but now we do it line by line
  /* buffer_append(&ab, "\x1b[2J", 4); */
  // postition cursor top left
  buffer_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);
  editor_draw_status_bar(&ab);
  editor_draw_message_bar(&ab);

  // position cursor to actual cursor position
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (ec.cy - ec.rowOffset) + 1,
           (ec.rx - ec.colOffset) + 1);
  buffer_append(&ab, buf, strlen(buf));

  // show cursor
  buffer_append(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  free_buffer(&ab);
}

int editor_read_key() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    /*
     * In Cygwin, when read() times out it returns -1 with an errno of EAGAIN,
     * instead of just returning 0 like it’s supposed to. To make it work in
     * Cygwin, we won’t treat EAGAIN as an error.
     * */
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  // handle escape sequences
  if (c == ESC) {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return ESC;
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return ESC;

    if (seq[0] == CSI) {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return ESC;
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          default:
            editor_set_status_msg("unknown [~ sequence %s", seq);
            break;
          }
        }

      } else {
        switch (seq[1]) {
        case 'A':
          return MOVE_CURSOR_UP;
        case 'B':
          return MOVE_CURSOR_DOWN;
        case 'C':
          return MOVE_CURSOR_RIGHT;
        case 'D':
          return MOVE_CURSOR_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        default:
          editor_set_status_msg("unknown [ sequence %s", seq);
          break;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      default:
        editor_set_status_msg("unknown O sequence %s", seq);
        break;
      }
    }
    return '\x1b';
  } else {
#ifdef VIM_MODE
    switch (c) {
    case 'k':
      return MOVE_CURSOR_UP;
    case 'j':
      return MOVE_CURSOR_DOWN;
    case 'l':
      return MOVE_CURSOR_RIGHT;
    case 'h':
      return MOVE_CURSOR_LEFT;
    case '0':
      return MOVE_CURSOR_START;
    case '$':
      return MOVE_CURSOR_END;
    }
#endif
    return c;
  }
}

void editor_move_cursor(int key, int times) {
  editor_row *row = ec.cy >= ec.numRows ? NULL : &ec.row[ec.cy];
  while (times--) {
    switch (key) {
    case MOVE_CURSOR_UP:
      if (ec.cy > 0) {
        ec.cy--;
      }
      break;
    case MOVE_CURSOR_DOWN:
      if (ec.cy + 1 < ec.numRows) {
        ec.cy++;
      }
      break;
    case MOVE_CURSOR_LEFT:
      if (ec.cx > 0) {
        ec.cx--;
      } else if (ec.cy > 0) {
        ec.cy--;
        ec.cx = ec.row[ec.cy].size;
      }
      break;
    case MOVE_CURSOR_RIGHT:
      if (row && ec.cx < row->size) {
        ec.cx++;
      } else if (row && ec.cx == row->size) {
        ec.cy++;
        ec.cx = 0;
      }
      break;
    case MOVE_CURSOR_START:
    case HOME_KEY:
      ec.cx = 0;
      break;
    case MOVE_CURSOR_END:
    case END_KEY:
      ec.cx = ec.row[ec.cy].size;
      break;
    }
  }
  row = ec.cy > ec.numRows ? NULL : &ec.row[ec.cy];

  int rowLen = row ? row->size : 1;
  if (ec.cx > rowLen) {
    ec.cx = rowLen;
  }
}

void editor_refresh_window_size() {
  if (term_get_window_size(&ec.screenRows, &ec.screenCols) == -1)
    die("editor_refresh_window_size");
  // setup offset for bottom bars
  ec.screenRows -= 2;
}

void editor_free_current_buffer() {
  ec.cx = 0;
  ec.cy = 0;
  ec.rx = 0;
  ec.rowOffset = 0;
  ec.colOffset = 0;
  ec.dirty = 0;
  if (ec.row != NULL && ec.numRows > 0) {
    for (int i = 0; i < ec.numRows; i++) {
      editor_free_row(&ec.row[i]);
    }
    free(ec.row);
  }
  ec.row = NULL;
  ec.numRows = 0;
}

void editor_init() {
  ec.cx = 0;
  ec.cy = 0;
  ec.rx = 0;
  ec.numRows = 0;
  ec.rowOffset = 0;
  ec.colOffset = 0;
  ec.dirty = 0;
  ec.filename = NULL;
  ec.statusmsg[0] = '\0';
  ec.statusmsg_time = 0;
  ec.syntax = NULL;
  editor_refresh_window_size();
  editor_set_status_msg("Hit Ctrl-Q to quit & Ctrl-Q to save");
}

void editor_process_keypress() {
  int c = editor_read_key();
  /* editor_set_status_msg("Key %02x pressed", c); */
  switch (c) {
  case 0:
  case ESC:
    // TODO??
    break;
  case CTRL_KEY('o'):
    editor_open();
    break;
  case CTRL_KEY('f'):
    editor_find();
    break;
  case CTRL_KEY('s'):
    editor_save();
    break;
  case CTRL_KEY(BACKSPACE):
    // TODO delete to start of line or delete line
    break;
  case '\r':
    editor_insert_newline();
    break;
  case BACKSPACE:
  case DEL_KEY:
  case CTRL_KEY('h'):
    if (c == DEL_KEY)
      editor_move_cursor(MOVE_CURSOR_RIGHT, 1);
    editor_delete_char();
    break;
  case CTRL_KEY('q'):
    // clear terminal
    if (editor_confirm() == 1) {
      term_clean();
      exit(0);
    }
    break;
  case MOVE_CURSOR_UP:
  case MOVE_CURSOR_DOWN:
  case MOVE_CURSOR_LEFT:
  case MOVE_CURSOR_RIGHT:
  case MOVE_CURSOR_START:
  case MOVE_CURSOR_END:
    editor_move_cursor(c, 1);
    break;
  case END_KEY:
    editor_move_cursor(MOVE_CURSOR_END, 1);
    break;
  case HOME_KEY:
    editor_move_cursor(MOVE_CURSOR_START, 1);
    break;
  case PAGE_UP: {
    editor_move_cursor(MOVE_CURSOR_UP, ec.screenRows);
    break;
  }
  case PAGE_DOWN: {
    editor_move_cursor(MOVE_CURSOR_DOWN, ec.rowOffset + ec.screenRows - 1);
    break;
  }
  default:
    if (c >= 0x20) {
      editor_insert_char(c);
    }
    break;
  }
}
