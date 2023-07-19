#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k)&0x1F)
#define ESC \x1b
#define CSI [

#define VERSION "0.0.1"
// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

#define TAB_SIZE 2

enum editorKeys {
  BACKSPACE = 127,
  MOVE_CURSOR_UP = 1000,
  MOVE_CURSOR_DOWN,
  MOVE_CURSOR_LEFT,
  MOVE_CURSOR_RIGHT,
  MOVE_CURSOR_START,
  MOVE_CURSOR_END,
  HOME_KEY,
  END_KEY,
  DEL_KEY,
  PAGE_UP,
  PAGE_DOWN,
};

typedef struct {
  char *chars;
  int size;
  char *render;
  int rsize;
} editorRow;

typedef struct {
  int cx, cy;
  int rx, ry;
  int screenCols;
  int screenRows;
  int numRows;
  int rowOffset;
  int colOffset;
  editorRow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios ogTermios;
} editorConfig;

editorConfig ec;

// append buffer used to construct a string to pass to write
typedef struct {
  char *b;
  int len;
} abuf;

#define ABUF_INIT                                                              \
  { NULL, 0 }

int getWindowSize(int *, int *);
void die(const char *);
int editorReadKey();
void updateRow(editorRow *row);
void editorInsertRow(int at, char *line, int linelen);
void editorFreeRow(editorRow *row);
void editorDeleteRow(int at);
void editorRowInsertChar(editorRow *row, int at, int c);
void editorInsertNewline();
void editorRowDeleteChar(editorRow *row, int at);
void editorSetStatusMessage(const char *fmt, ...);
char *editorRowsToString(int *len);
void editorRowAppendString(editorRow *row, char *s, size_t len);
void editorMoveCursor(int key);
void editorSave();
void editorRefreshScreen();
char *editorPrompt(char *prompt);

void abAppend(abuf *ab, const char *s, int len) {
  char *buf = realloc(ab->b, ab->len + len);

  if (buf == NULL)
    return;
  memcpy(&buf[ab->len], s, len);
  ab->b = buf;
  ab->len += len;
}

void abFree(abuf *ab) { free(ab->b); }

char *editorPrompt(char *prompt) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == '\x1b') {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == BACKSPACE || c == DEL_KEY || c == CTRL_KEY('h')) {
      if (buflen > 0) {
        buf[--buflen] = '\0';
      }
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
  }
}

void editorSave() {
  if (ec.filename == NULL) {
    ec.filename = editorPrompt("Save as: %s (ESC to cancel)");
    if (ec.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
  }
  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(ec.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        ec.dirty = 0;
        editorSetStatusMessage("%d bytes writen to \"%s\"", len, ec.filename);
        return;
      }
    }
    close(fd);
  }

  free(buf);
  editorSetStatusMessage("I/O error while saving using editorSave(): %s",
                         strerror(errno));
}

char *editorRowsToString(int *buflen) {
  int len = 0;
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

void editorInsertNewline() {
  if (ec.cx == 0) {
    editorInsertRow(ec.cy, "", 0);
  } else {
    editorRow *row = &ec.row[ec.cy];
    editorInsertRow(ec.cy + 1, &row->chars[ec.cx], row->size - ec.cx);
    row = &ec.row[ec.cy];
    row->size = ec.cx;
    row->chars[row->size] = '\0';
    updateRow(row);
  }
  ec.cy++;
  ec.cx = 0;
}

void editorRowInsertChar(editorRow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  updateRow(row);
  ec.dirty++;
}

void editorRowDeleteChar(editorRow *row, int at) {
  if (row == NULL || at < 0 || at > row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->chars[row->size] = '\0';
  editorSetStatusMessage("ec.cx: %d | rsize %d | at %d", ec.cx,
                         ec.row[ec.cy].size, at);
  row->size--;
  updateRow(row);
  ec.dirty++;
}

void editorDelChar() {
  if ((ec.cy == ec.numRows) || (ec.cx == 0 && ec.cy == 0))
    return;

  editorRow *row = &ec.row[ec.cy];
  if (ec.cx > 0) {
    editorRowDeleteChar(row, ec.cx - 1);
    ec.cx--;
  } else {
    ec.cx = ec.row[ec.cy - 1].size;
    editorRowAppendString(&ec.row[ec.cy - 1], row->chars, row->size);
    editorDeleteRow(ec.cy);
    ec.cy--;
  }
}

void editorInsertChar(int c) {
  if (ec.cy == ec.numRows) {
    editorInsertRow(ec.numRows, "", 0);
  }
  int len = ec.row[ec.cy].rsize;

  editorRowInsertChar(&ec.row[ec.cy], ec.cx, c);
  int offset = ec.row[ec.cy].rsize - len;
  ec.cx += offset >= 0 ? offset : 0;
}

int editorRowCxToRx(editorRow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TAB_SIZE - 1) - (rx % TAB_SIZE);
    rx++;
  }

  return rx;
}

void updateRow(editorRow *row) {
  int j;

  // suport tabs
  int tabs = 0;
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
}

void editorInsertRow(int at, char *line, int linelen) {
  if (at < 0 || at > ec.numRows)
    return;
  // realloc a new row
  ec.row = realloc(ec.row, sizeof(editorRow) * (ec.numRows + 1));
  memmove(&ec.row[at + 1], &ec.row[at], sizeof(editorRow) * (ec.numRows - at));
  ec.row[at].size = linelen;
  ec.row[at].chars = malloc(linelen + 1);

  memcpy(ec.row[at].chars, line, linelen);
  ec.row[at].chars[linelen] = '\0';
  ec.row[at].rsize = 0;
  ec.row[at].render = NULL;

  updateRow(&ec.row[at]);
  ec.numRows++;
  ec.dirty++;
}

void editorRowAppendString(editorRow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  updateRow(row);
  ec.dirty++;
}

void editorFreeRow(editorRow *row) {
  free(row->render);
  free(row->chars);
}

void editorDeleteRow(int at) {
  if (at < 0 || at >= ec.numRows)
    return;
  editorFreeRow(&ec.row[at]);
  memmove(&ec.row[at], &ec.row[at + 1],
          sizeof(editorRow) * (ec.numRows - at - 1));
  ec.numRows--;
  ec.dirty++;
}

void editorOpen(char *filename) {
  free(ec.filename);
  ec.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  int linelen;

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {

      linelen--;
    }
    editorInsertRow(ec.numRows, line, linelen);
  }

  if (ec.numRows == 0) {
    editorInsertRow(0, "", 0);
  }

  free(line);
  fclose(fp);

  ec.dirty = 0;
}

void editorDrawRows(abuf *ab) {
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
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, message, messageLen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = ec.row[fileRow].rsize - ec.colOffset;
      len = len < 0 ? 0 : len;
      len = len > ec.screenCols ? ec.screenCols : len;
      abAppend(ab, &ec.row[fileRow].render[ec.colOffset], len);
    }
    // clear from cursor to end of line
    abAppend(ab, "\x1b[K", 3);
    // clear line
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     ec.filename ? ec.filename : "[No Name]", ec.numRows,
                     ec.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "[%d/%d] %d/%d", ec.cx, ec.cy,
                      ec.cy + 1, ec.numRows);
  if (len > ec.screenCols)
    len = ec.screenCols;
  abAppend(ab, status, len);
  while (len < ec.screenCols) {
    if (ec.screenCols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(ec.statusmsg);
  if (msglen > ec.screenCols)
    strlen(ec.statusmsg);
  if (msglen && time(NULL) - ec.statusmsg_time < 5)
    abAppend(ab, ec.statusmsg, msglen);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ec.statusmsg, sizeof(ec.statusmsg), fmt, ap);
  va_end(ap);
  ec.statusmsg_time = time(NULL);
}

void editorScroll() {
  ec.rx = 0;

  if (ec.cy < ec.numRows) {
    ec.rx = editorRowCxToRx(&ec.row[ec.cy], ec.cx);
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

void editorRefreshScreen() {
  editorScroll();
  abuf ab = ABUF_INIT;

  // https://vt100.net/docs/vt100-ug/chapter3.html
  // for details on escape sequence
  // hide cursor to prevent flickering
  abAppend(&ab, "\x1b[?25l", 6);
  // this one basically means clear the entire screen
  // but now we do it line by line
  /* abAppend(&ab, "\x1b[2J", 4); */
  // postition cursor top left
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  // position cursor to actual cursor position
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (ec.cy - ec.rowOffset) + 1,
           (ec.rx - ec.colOffset) + 1);
  abAppend(&ab, buf, strlen(buf));

  // show cursor
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void die(const char *s) {
  perror(s);
  exit(1);
}

int editorReadKey() {
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
  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
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
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    /* switch (c) { */
    /*   case 'k': */
    /*     return MOVE_CURSOR_UP; */
    /*   case 'j': */
    /*     return MOVE_CURSOR_DOWN; */
    /*   case 'l': */
    /*     return MOVE_CURSOR_RIGHT; */
    /*   case 'h': */
    /*     return MOVE_CURSOR_LEFT; */
    /*   case '0': */
    /*     return MOVE_CURSOR_START; */
    /*   case '$': */
    /*     return MOVE_CURSOR_END; */
    /* } */
    return c;
  }
}

void editorMoveCursor(int key) {
  editorRow *row = ec.cy >= ec.numRows ? NULL : &ec.row[ec.cy];
  switch (key) {
  case MOVE_CURSOR_UP:
    if (ec.cy > 0) {
      ec.cy--;
    }
    break;
  case MOVE_CURSOR_DOWN:
    if (ec.cy < ec.numRows) {
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
    if (ec.cy < ec.numRows)
      ec.cx = ec.row[ec.cy].size;
    break;
  }

  row = ec.cy >= ec.numRows ? NULL : &ec.row[ec.cy];
  int rowLen = row ? row->size : 0;
  if (ec.cx > rowLen) {
    ec.cx = rowLen;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  // send escape sequence to request cursor position
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  // parse response
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void refreshWindowSize() {
  if (getWindowSize(&ec.screenRows, &ec.screenCols) == -1)
    die("getWindowSize");
  // setup offset for bottom bars
  ec.screenRows -= 2;
}

void initEditor() {
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
  refreshWindowSize();
}

void editorProcessKeypress() {
  int c = editorReadKey();
  editorSetStatusMessage("Key %02x pressed", c);
  // ignore weird chars
  /* if (c != 0x1B && c < 0x20) { */
  /* } */
  switch (c) {
  case 0:
    // TODO??
    break;
  case CTRL_KEY('s'):
    // TODO
    editorSave();
    break;
  case '\r':
    editorInsertNewline();
    break;
  case BACKSPACE:
  case DEL_KEY:
  case CTRL_KEY('h'):
    if (c == DEL_KEY)
      editorMoveCursor(MOVE_CURSOR_RIGHT);
    editorDelChar();
    break;
  case CTRL_KEY('l'):
  case '\x1b':
    // TODO
    break;
  case CTRL_KEY('q'):
    // clear terminal
    fputs("\033c", stdout);
    exit(0);
    break;
  case MOVE_CURSOR_UP:
  case MOVE_CURSOR_DOWN:
  case MOVE_CURSOR_LEFT:
  case MOVE_CURSOR_RIGHT:
  case MOVE_CURSOR_START:
  case MOVE_CURSOR_END:
    editorMoveCursor(c);
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      ec.cy = ec.rowOffset;
    } else if (c == PAGE_DOWN) {
      ec.cy = ec.rowOffset + ec.screenRows - 1;
      if (ec.cy > ec.numRows)
        ec.cy = ec.numRows;
    }
    int times = ec.screenRows;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? MOVE_CURSOR_UP : MOVE_CURSOR_DOWN);
    break;
  }
    editorInsertChar(c);
    break;
  default:
    if (c >= 0x20) {
      editorInsertChar(c);
    }
    break;
  }
}

void resetTerminalOptions() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ec.ogTermios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  atexit(resetTerminalOptions);

  struct termios raw;

  if (tcgetattr(STDIN_FILENO, &raw) == -1)
    die("tcgetattr");

  /*
   * IXON: turn off software control flow data features (Ctrl-S/Ctrl-Q to stop
   * and resume program output) ICRNL: turn off carriage return new line
   * features
   * */
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

  /*
   * OPOST: disable post process on ouput
   * */
  raw.c_oflag &= ~(OPOST);

  /*
   * CS8: character size per byte to 8bits
   * */
  raw.c_cflag &= ~(CS8);

  /*
   * ECHO: turn off echoing input
   * ICANON: turnoff canonical mode aka read byte per byte instead of line per
   * line ISIG: turn off term signals like Ctrl-Z, etc.. IEXTEN: turn off
   * Ctrl-V
   * */
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  // sets a timeout to read()
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int main(int argc, char *argv[]) {
  tcgetattr(STDIN_FILENO, &ec.ogTermios);
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  /* editorSetStatusMessage("Hit Ctrl-Q to quit"); */

  while (1) {

    refreshWindowSize();
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

/* https://viewsourcecode.org/snaptoken/kilo/05.aTextEditor.html */
