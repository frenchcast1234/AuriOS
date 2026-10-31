#include "../include/keyboard.h"
#include "../include/commands.h"
#include "../include/terminal.h"
#include "../include/integer.h"
#include "../include/keymap.h"
#include "../include/string.h"

char *keymap_index_names[] = {
    "us",
    "fr"
};
int index = 0;
void keyboard(void *args, int argc) {
    char **argv = (char **)args;
    if (argc != 2) {
        terminal_writestring("usage: keyboard <layout>\n");
        terminal_writestring("layout us : qwerty\n");
        terminal_writestring("layout fr : azerty\n");
        return;
    }


    if (strcmp(argv[1], "us") == 0 || strcmp(argv[1], "US") == 0) {
        keymap_set_index(0);
        terminal_writestring("Keyboard layout set to US (QWERTY)\n");
        return;
    } else if (strcmp(argv[1], "fr") == 0 || strcmp(argv[1], "FR") == 0) {
        keymap_set_index(1);
        terminal_writestring("Keyboard layout set to FR (AZERTY)\n");
        return;
    }

    keymap_set_index(index);
}