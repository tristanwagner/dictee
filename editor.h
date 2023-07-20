#ifndef _EDITOR_H_
#define _EDITOR_H_
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/utils/src/buffer.h"
#include "include/utils/src/term.h"
#include "include/utils/src/uerror.h"

#define CTRL_KEY(k) ((k)&0x1F)
#define VERSION "0.0.1"

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
} editor_row;

//TODO extract buffer/file stuff
//to be able to support multiple files
typedef struct {
  int cx, cy;
  int rx, ry;
  int screenCols;
  int screenRows;
  int numRows;
  int rowOffset;
  int colOffset;
  editor_row *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
} editor_config;

int editor_read_key();
void editor_update_row(editor_row *row);
void editor_insert_row(int at, char *line, int linelen);
void editor_delete_row(int at);
void editor_free_row(editor_row *row);
void editor_insert_char(int c);
void editor_insert_newline();
void editor_row_delete_char(editor_row *row, int at);
void editor_delete_char();
void editor_set_status_msg(const char *fmt, ...);
char *editor_rows_to_string(size_t *len);
void editor_row_append_string(editor_row *row, char *s, size_t len);
void editor_move_cursor(int key, int times);
void editor_row_insert_char(editor_row *row, int at, int c);
int editor_row_cx_to_rx(editor_row *row, int cx);
int editor_save_file(const char *filename, char *buffer, ssize_t len);
void editor_save();
int editor_confirm();
void editor_refresh_screen();
void editor_init();
// TODO: implement command for opening file inside the editor
void editor_open(char *filename);
void editor_refresh_window_size();
void editor_process_keypress();
char *editor_prompt(char *prompt);

#endif
