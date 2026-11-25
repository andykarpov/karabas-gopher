// minimal test helpers for the host tests
#pragma once
#include <stdio.h>
#include <string.h>
static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); checks++; \
  if (_a != _b) { fails++; printf("%s:%d: %s = 0x%lX, want 0x%lX\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)
#define CHECK_STR(a, b) do { checks++; if (strcmp((a), (b))) { fails++; \
  printf("%s:%d: %s = \"%s\", want \"%s\"\n", __FILE__, __LINE__, #a, (a), (b)); } } while (0)
#define DONE() do { printf("%s: %d checks, %d failed\n", __FILE__, checks, fails); return fails != 0; } while (0)
