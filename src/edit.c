#include <string.h>
#include "edit.h"
#include "kbd.h"

void edit_start(edit_t *e, char *buf, uint8_t max, uint8_t width)
{
  e->buf = buf;
  e->max = max;
  e->len = e->pos = (uint8_t)strlen(buf);
  e->width = width;
  e->scroll = 0;
  e->ovr = 0;
  edit_key(e, K_NONE);
}

uint8_t edit_key(edit_t *e, uint16_t key)
{
  char *b = e->buf;
  switch (key) {
    case K_ENTER: return EDIT_DONE;
    case K_ESC:   return EDIT_CANCEL;
    case K_LEFT:  if (e->pos) e->pos--; break;
    case K_RIGHT: if (e->pos < e->len) e->pos++; break;
    case K_HOME:  e->pos = 0; break;
    case K_END:   e->pos = e->len; break;
    case K_BS:
      if (!e->pos) break;
      e->pos--;
      /* fall through */
    case K_DEL:
      if (e->pos < e->len) {
        memmove(b + e->pos, b + e->pos + 1, e->len - e->pos);
        e->len--;
      }
      break;
    case K_DEL | KM_CTRL:              // Ctrl+Del / Ctrl+Y: clear the line
    case 'y' | KM_CTRL:
      e->len = e->pos = 0;
      b[0] = 0;
      break;
    default:
      if (key < 0x20 || key > 0xFF) break;
      if (e->ovr && e->pos < e->len) {
        b[e->pos++] = (char)key;
      } else if (e->len < e->max) {
        memmove(b + e->pos + 1, b + e->pos, e->len - e->pos + 1);
        b[e->pos++] = (char)key;
        e->len++;
      }
  }
  if (e->pos < e->scroll) e->scroll = e->pos;
  if (e->width && e->pos >= e->scroll + e->width) e->scroll = e->pos - e->width + 1;
  return EDIT_GOING;
}
