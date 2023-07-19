#include "editor.h"
#include "term.h"

int main(int argc, char *argv[]) {
  term_init();
  editor_init();
  if (argc >= 2) {
    editor_open(argv[1]);
  }

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }

  return 0;
}
