#include "editor.h"

int main(int argc, char *argv[]) {
  term_init();
  editor_init();

  if (argc >= 2) {
    editor_open_file(argv[1]);
  }

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }

  editor_exit();
  return 0;
}
