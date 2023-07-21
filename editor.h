#ifndef _EDITOR_H_
#define _EDITOR_H_
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "include/utils/src/buffer.h"
#include "include/utils/src/term.h"
#include "include/utils/src/uerror.h"
#include "include/utils/src/ustring.h"

#define CTRL_KEY(k) ((k)&0x1F)

#define VERSION "0.0.1"

#define TAB_SIZE 2

enum editor_keys {
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

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)
#define HL_HIGHLIGHT_COMMENT (1<<2)

enum editor_highlight {
  HL_DEFAULT = 0,
  HL_NUMBER,
  HL_STRING,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_SEARCH_RESULT,
};

typedef struct {
  char* filetype;
  char** filematches;
  char* single_line_comment_start;
  char* multiline_comment_start;
  char* multiline_comment_end;
  char** keywords;
  int flags;
} editor_syntax;

typedef struct {
  int index;
  char *chars;
  int size;
  char *render;
  int rsize;
  unsigned char* hl;
  int hl_open_comment;
} editor_row;

typedef struct {
  int cx, cy;
  int rowOffset;
  int colOffset;
} editor_cursor_position;

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
  editor_syntax *syntax;
} editor_config;

void editor_save_cursor_position();
void editor_restore_cursor_position();
int editor_read_key();
void editor_update_syntax();
void editor_row_update_syntax(editor_row *row);
int editor_syntax_to_color(int hl);
void editor_select_filetype_syntax();
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
int editor_row_rx_to_cx(editor_row *row, int rx);
int editor_save_file(const char *filename, char *buffer, ssize_t len);
void editor_save();
int editor_confirm();
void editor_refresh_screen();
void editor_init();
void editor_open_file(char *filename);
void editor_open();
void editor_free_current_buffer();
void editor_refresh_window_size();
void editor_process_keypress();
char *editor_prompt(char *prompt, void (*callback)(char *, int));

#endif
