#include<errno.h>
#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<termios.h>

#define CTRL_KEY(k) ((k) & 0x1F)
#define VERSION "0.0.1"
// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

int getWindowSize(int*, int*);
void die(const char*);

enum editorKeys {
  MOVE_CURSOR_UP = 1000,
  MOVE_CURSOR_DOWN,
  MOVE_CURSOR_LEFT,
  MOVE_CURSOR_RIGHT,
  MOVE_CURSOR_START,
  MOVE_CURSOR_END,
  PAGE_UP,
  PAGE_DOWN,
};

struct editorConfig {
  int cx, cy;
  int screenCols;
  int screenRows;
  struct termios ogTermios;
};

struct editorConfig ec;

// append buffer used to construct a string to pass to write
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *buf = realloc(ab->b, ab->len + len);

  if (buf == NULL) return;
  memcpy(&buf[ab->len], s, len);
  ab->b = buf;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < ec.screenRows; y++) {
    if (y == ec.screenRows / 3) {
      char message[64];
      int messageLen = snprintf(message, sizeof(message), "Dictée - version %s", VERSION);
      if (messageLen > ec.screenCols) messageLen = ec.screenCols;
      int padding = (ec.screenCols - messageLen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, message, messageLen);
    } else {
      abAppend(ab, "~", 1);
    }
    // clear from cursor to end of line
    abAppend(ab, "\x1b[K", 3);
    // clear line
    if (y < ec.screenRows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

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

  // position cursor to actual cursor position
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", ec.cy + 1, ec.cx + 1);
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
     * In Cygwin, when read() times out it returns -1 with an errno of EAGAIN, instead of just returning 0 like it’s supposed to. To make it work in Cygwin, we won’t treat EAGAIN as an error.
     * */
    if(nread == -1 && errno != EAGAIN) die("read");
  }

  // handle escape sequences
  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch(seq[1]) {
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
          }
        }

      } else {
        switch(seq[1]) {
          case 'A': return MOVE_CURSOR_UP;
          case 'B': return MOVE_CURSOR_DOWN;
          case 'C': return MOVE_CURSOR_RIGHT;
          case 'D': return MOVE_CURSOR_LEFT;
        }
      }
    }
    return '\x1b';
  } else {
    switch(c) {
      case '0': return MOVE_CURSOR_START;
      case '$': return MOVE_CURSOR_END;
    }
    return c;
  }
}

void editorMoveCursor(int key) {
  switch (key) {
    case MOVE_CURSOR_UP:
      if (ec.cy > 0) {
        ec.cy--;
      }
      break;
    case MOVE_CURSOR_DOWN:
      if (ec.cy < ec.screenRows - 1) {
        ec.cy++;
      }
      break;
    case MOVE_CURSOR_LEFT:
      if (ec.cx > 0) {
        ec.cx--;
      }
      break;
    case MOVE_CURSOR_RIGHT:
      if (ec.cx < ec.screenCols - 1) {
        ec.cx++;
      }
      break;
    case MOVE_CURSOR_START:
      ec.cx = 0;
      break;
    case MOVE_CURSOR_END:
      ec.cx = ec.screenCols - 1;
      break;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  // send escape sequence to request cursor position
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  // parse response
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void initEditor() {
  ec.cx = 0;
  ec.cy = 0;
  if (getWindowSize(&ec.screenRows, &ec.screenCols) == -1) die("getWindowSize");
}

void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
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
      int times = ec.screenRows;
      while (times--) editorMoveCursor(c == PAGE_UP ? MOVE_CURSOR_UP : MOVE_CURSOR_DOWN);
      break;
    }
  }
}

void resetTerminalOptions() {
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &ec.ogTermios) == -1) die ("tcsetattr");
}

void enableRawMode() {
  atexit(resetTerminalOptions);

  struct termios raw;

  if (tcgetattr(STDIN_FILENO, &raw) == -1) die("tcgetattr");

  /*
   * IXON: turn off software control flow data features (Ctrl-S/Ctrl-Q to stop and resume program output)
   * ICRNL: turn off carriage return new line features
   * */
  raw.c_iflag &= ~(IXON|ICRNL|BRKINT|INPCK|ISTRIP);

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
   * ICANON: turnoff canonical mode aka read byte per byte instead of line per line
   * ISIG: turn off term signals like Ctrl-Z, etc..
   * IEXTEN: turn off Ctrl-V
   * */
  raw.c_lflag &= ~(ECHO|ICANON|ISIG|IEXTEN);

  // sets a timeout to read()
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int main () {
  tcgetattr(STDIN_FILENO, &ec.ogTermios);
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

// https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html step 34
