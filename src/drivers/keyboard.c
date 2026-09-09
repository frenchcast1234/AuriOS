#include "../include/keyboard.h"
#include "../include/isr.h"
#include "../include/io.h"
#include "../include/terminal.h"
#include "../include/shell.h"
#include "../include/log.h"
#include "../include/pic.h"
#include "../include/history.h"

static char scancode_to_ascii[8][128] = {
    {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
        0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    },
    {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
        0, 'a', 'z', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'w', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    }
};

static char scancode_to_ascii_shift[8][128] = {
    {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
        0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0, ' '
    },
    {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
        0, 'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'Q', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|', 'W', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0, ' '
    }
};

static int shift_pressed = 0;
static int caps_lock = 0;
static int ctrl_pressed = 0;
static int extended = 0;
static int current_keyboard = 0;
static int alt_pressed = 0;

void keyboard_callback(registers_t *regs)
{
    (void)regs;
    uint8_t scancode = inb(0x60);

	//extended-key
	if (scancode == 0xE0) {
		extended = 1;
		return;
	}
	if (extended) {
		extended = 0;
        // Up captured
		if (scancode == 0x48) {
            if (alt_pressed) 
                shell_home();
            else
			    shell_history(1);
			return;
		}
		// Down captured
		if (scancode == 0x50) {
            if (alt_pressed)
                shell_end();
            else
			    shell_history(2);
			return;
		}
        // Left captured
        if (scancode == 0x4B) {
            shell_buffer_pos_decrement();
            return;
        }
        // Right captured
        if (scancode == 0x4D) {
            shell_buffer_pos_increment();
            return;
        }
        // delete
        if (scancode == 0x53) {
            shell_delete();
            return;
        }
	}

    // Caps lock captured
    if (scancode == 0x3A) {
        if (caps_lock == 0) caps_lock = 1;
        else caps_lock = 0;
        return;
    }
	
    // Shift captured
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    // Shift free
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }
    // Ctrl captured
    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }

    // Ctrl free
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    // Alt captured
    if (scancode == 0x38) {
        alt_pressed = 1;
        return;
    }
    // Alt free
    if (scancode == 0xB8) {
        alt_pressed = 0;
        return;
    }
    
    if (scancode & 0x80)
        return;
    
    // Ctrl+L
    if (ctrl_pressed && scancode == 0x26) {
        shell_handle_key('\f');
        return;
    }

    // Backspace
    if (scancode == 0x0E) {
        shell_handle_key('\b');
        return;
    }
    char c ;

    if (scancode == 0x0F) {
        shell_handle_key('\t');
        return;
    }
    
    if (shift_pressed || caps_lock)
        c = scancode_to_ascii_shift[current_keyboard][scancode];
    else
        c = scancode_to_ascii[current_keyboard][scancode];
    if (c)
        shell_handle_key(c);
}

void keyboard_set_current_keyboard(int index) {
    current_keyboard = index;
}

void keyboard_init(void)
{
    irq_register_handler(1, keyboard_callback);
    pic_unmask_irq(1);
    KINFO("[KBD] PS/2 Keyboard driver active");
}
