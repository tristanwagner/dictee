#include "term.h"

static struct termios *lptr;

void term_reset_options() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, lptr) == -1)
    die("tcsetattr");
}

void term_enable_raw_mode() {
  atexit(term_reset_options);

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

void term_init() {
  tcgetattr(STDIN_FILENO, lptr);
  term_enable_raw_mode();
}

int term_get_window_size(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return term_get_cursor_position(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

int term_get_cursor_position(int *rows, int *cols) {
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

void term_clean() { fputs("\033c", stdout); }

void term_pre_quit() {
  term_clean();
  term_reset_options();
}
