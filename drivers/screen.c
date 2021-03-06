#include "screen.h"
#include "../cpu/ports.h"
#include "../libc/mem.h"


int get_offset_row(int offset) {
  return offset / (2 * MAX_COLS);
}

int get_offset_col(int offset) {
  return (offset - (get_offset_row(offset)*2*MAX_COLS))/2;
}

void delete_last() {
    int offset = get_cursor()-2;
    int row = get_offset_row(offset);
    int col = get_offset_col(offset);
    print_char(0x08, col, row, WHITE_ON_BLACK);
}

int get_screen_offset(int col, int row) {
  return (row * MAX_COLS + col) * 2;
}

int get_cursor() {
  // The device uses its control register as an index
  // to select its internal registers, of which we are
  // interested in:
  port_byte_out(REG_SCREEN_CTRL, 14);
  int offset = port_byte_in(REG_SCREEN_DATA) << 8;
  port_byte_out(REG_SCREEN_CTRL, 15);
  offset += port_byte_in(REG_SCREEN_DATA);
  // Since the cursor offset reported by the VGA hardware is the
  // number of characters, we multiply by two to convert it to
  // a character cell offset.
  return offset * 2;
}

void set_cursor(int offset) {
  offset /= 2;
  // This is similar to get_cursor, only now we write
  // bytes to those internal device registers.

  port_byte_out(REG_SCREEN_CTRL, 14);
  port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
  /* port_byte_out(REG_SCREEN_DATA, (unsigned char)0); */
  port_byte_out(REG_SCREEN_CTRL, 15);
  port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
  /* port_byte_out(REG_SCREEN_DATA, (unsigned char)0); */
}

int handle_scrolling(int cursor_offset) {
  if (cursor_offset < MAX_ROWS*MAX_COLS*2) {
    return cursor_offset;
  }

  int i;
  for (i=1; i<MAX_ROWS; i++) {
    memory_copy(
                (char*)get_screen_offset(0,i) + VIDEO_ADDRESS,
                (char*)get_screen_offset(0,i-1) + VIDEO_ADDRESS,
                MAX_COLS *2);
  }
  char* last_line = (char*)get_screen_offset(0,MAX_ROWS -1) + VIDEO_ADDRESS;
  for (i=0; i < MAX_COLS*2; i++) {
    last_line[i] = 0;
  }
  cursor_offset -= 2*MAX_COLS;
  return cursor_offset;
}

void print_at(char* message, int col, int row) {
  // Update the cursor if col and row not negative.
  if (col >= 0 && row >= 0) {
    set_cursor(get_screen_offset(col, row));
  }

  // Loop through each char of the message and print it.
  int i = 0;
  while(message[i] != 0) {
    print_char(message[i], col + i, row, WHITE_ON_BLACK);
    i += 1;
  }
}


void print_char(char character, int col, int row, char attr) {
  /* Create a byte ( char ) pointer to the start of video memory */
  unsigned char* vidmem = (unsigned char*) VIDEO_ADDRESS;

  /* If attribute byte is zero , assume the default style . */
  if (!attr) {
    attr = WHITE_ON_BLACK;
  }

  /* Get the video memory offset for the screen location */
  int offset;
  /* If col and row are non - negative , use them for offset . */
  if (col >= 0 && row >= 0) {
    offset = get_screen_offset(col, row);
  } else {
    offset = get_cursor();
  }

  //   If we see a newline character , set offset to the end of
  //   current row , so it will be advanced to the first col
  //   of the next row .
  if (character == '\n') {
    int rows = offset / (2 * MAX_COLS);
    offset = get_screen_offset(79, rows);
    // Update the offset to the next character cell , which is
    // two bytes ahead of the current cell .
    offset += 2;
    // Otherwise , write the character and its attribute byte to
    // video memory at our calculated offset .
  } else if (character == 0x08) { // Backspace
    vidmem[offset] = ' ';
    vidmem[offset + 1] = attr;

  } else {
    vidmem[offset] = character;
    vidmem[offset + 1] = attr;
    // Update the offset to the next character cell , which is
    // two bytes ahead of the current cell .
    offset += 2;
  }

  // Make scrolling adjustment , for when we reach the bottom
  // of the screen .
  offset = handle_scrolling(offset);
  // Update the cursor position on the screen device .
  set_cursor(offset);
}
