#include "../include/terminal.h"
#include "../include/string.h"
#include "../include/ansi.h"
#include "../include/io.h"
#ifdef AURI_TEST_MODE
      #include "../include/serial.h"
#endif
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define SCROLLBACK_LINES 256

#define VGA_CTRL_REGISTER 0x3D4
#define VGA_DATA_REGISTER 0x3D5
#define VGA_OFFSET_LOW    0x0F
#define VGA_OFFSET_HIGH   0x0E
#define VGA_CURSOR_START  0x0A
#define VGA_CURSOR_END    0x0B

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
static uint16_t terminal_screen[VGA_WIDTH * VGA_HEIGHT];
static uint16_t* terminal_buffer = terminal_screen;
static uint16_t terminal_scrollback[SCROLLBACK_LINES][VGA_WIDTH];
static size_t terminal_scrollback_total;
static size_t terminal_view_offset;
static int terminal_cursor_hidden;

static inline uint16_t vga_entry(unsigned char uc, uint8_t color);

static void terminal_render(void) {
    uint16_t* vga = (uint16_t*) VGA_MEMORY;
    const size_t oldest = terminal_scrollback_total > SCROLLBACK_LINES
        ? terminal_scrollback_total - SCROLLBACK_LINES : 0;
    const size_t first_line = terminal_scrollback_total - terminal_view_offset;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        const size_t line = first_line + y;
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            uint16_t entry = vga_entry(' ', terminal_color);
            if (line < terminal_scrollback_total && line >= oldest) {
                entry = terminal_scrollback[line % SCROLLBACK_LINES][x];
            } else if (line >= terminal_scrollback_total &&
                       line < terminal_scrollback_total + VGA_HEIGHT) {
                entry = terminal_screen[(line - terminal_scrollback_total) * VGA_WIDTH + x];
            }
            vga[y * VGA_WIDTH + x] = entry;
        }
    }
}

void terminal_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
    outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xC0) | cursor_start);
 
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_END);
    outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xE0) | cursor_end);
}

void terminal_update_cursor(size_t x, size_t y) {
    if (terminal_view_offset != 0) {
        if (!terminal_cursor_hidden) {
            outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
            outb(VGA_DATA_REGISTER, 0x20);
            terminal_cursor_hidden = 1;
        }
        return;
    }

    if (terminal_cursor_hidden) {
        terminal_enable_cursor(0, 15);
        terminal_cursor_hidden = 0;
    }

    uint16_t pos = y * VGA_WIDTH + x;

    outb(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
    outb(VGA_DATA_REGISTER, (uint8_t)(pos & 0xFF));

    outb(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
    outb(VGA_DATA_REGISTER, (uint8_t)((pos >> 8) & 0xFF));
}

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;    
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_scrollback_total = 0;
    terminal_view_offset = 0;
    terminal_cursor_hidden = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_render();
    terminal_enable_cursor(0, 15);
    terminal_update_cursor(terminal_column, terminal_row);
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
    if (terminal_view_offset == 0) {
        ((uint16_t*) VGA_MEMORY)[index] = terminal_buffer[index];
    }
}

static void terminal_scroll(void) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_scrollback[terminal_scrollback_total % SCROLLBACK_LINES][x] =
            terminal_buffer[x];
    }
    terminal_scrollback_total++;
    if (terminal_view_offset != 0 &&
        terminal_view_offset < SCROLLBACK_LINES) {
        terminal_view_offset++;
    }

    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] = terminal_buffer[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_render();
}


void terminal_putchar(char c) {
  ansi_process_char((uint8_t)c);
  #ifdef AURI_TEST_MODE
    serial_write_char(c);
  #endif
}

void terminal_putchar_raw(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
        terminal_update_cursor(terminal_column, terminal_row);
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);

    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }
    terminal_update_cursor(terminal_column, terminal_row);
}

void terminal_print_hex(uint32_t val) {
	char buf[11];
	buf[0] = '0';
	buf[1] = 'x';
	for (int i = 9; i >= 2; i--) {
		uint8_t nibble = val & 0xF;
		buf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
		val >>= 4;
	}
	buf[10] = '\0';
	terminal_writestring(buf);
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}


void terminal_clear(void){
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color) ;
        }
    } 
    terminal_row = 0;
    terminal_column = 0;
    terminal_scrollback_total = 0;
    terminal_view_offset = 0;
    terminal_render();
    terminal_update_cursor(terminal_column, terminal_row);
}

void terminal_clear_until_cursor(void) {
    size_t limit = terminal_row * VGA_WIDTH + terminal_column;
    for (size_t i = 0; i < limit; i++)
      terminal_buffer[i] = vga_entry(' ', terminal_color);
    terminal_render();
}

void terminal_clear_after_cursor(void) {
    size_t limit = VGA_WIDTH * VGA_HEIGHT;
    size_t start = terminal_row * VGA_WIDTH + terminal_column;
    for (size_t i = start; i < limit; i++)
      terminal_buffer[i] = vga_entry(' ', terminal_color);
    terminal_render();
}

void terminal_set_cursor(size_t x, size_t y) {
  if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
  if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

  terminal_column = x;
  terminal_row = y;
  terminal_update_cursor(terminal_column, terminal_row);
}

void terminal_backspace(void) {
    if (terminal_column > 0) {
        terminal_column--;
    }
    else if (terminal_row > 0) {
        terminal_row--;
        terminal_column = 79;
    }
    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    terminal_update_cursor(terminal_column, terminal_row);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

void terminal_move_cursor(int offset) {
    int pos = (int)(terminal_row * VGA_WIDTH + terminal_column) + offset;
    if (pos < 0) pos = 0;
    if (pos >= (int)(VGA_WIDTH * VGA_HEIGHT)) pos = VGA_WIDTH * VGA_HEIGHT - 1;
    terminal_row = pos / VGA_WIDTH;
    terminal_column = pos % VGA_WIDTH;
    terminal_update_cursor(terminal_column, terminal_row);
}

void terminal_scroll_view(int lines) {
    const size_t available = terminal_scrollback_total < SCROLLBACK_LINES
        ? terminal_scrollback_total : SCROLLBACK_LINES;

    if (lines > 0) {
        size_t requested = (size_t) lines;
        terminal_view_offset = requested > available - terminal_view_offset
            ? available : terminal_view_offset + requested;
    } else if (lines < 0) {
        size_t requested = (size_t) -lines;
        terminal_view_offset = requested > terminal_view_offset
            ? 0 : terminal_view_offset - requested;
    }

    terminal_render();
    terminal_update_cursor(terminal_column, terminal_row);
}
