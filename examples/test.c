#include <stdio.h>

void test0() {
}

void (*test1(void))() {
  return nullptr;
}
void test2() {
}

char c() {

}

void *test3() {
  return nullptr;
}

int main(void) {
  int x = 100;
  char c = 'c';

  int y = x << x;

  y >>= 10;

  int z = 0xFF;

  int t = 0b10;

  char *a = "Hello world";

  for (t++; a < 0; a++) {

  }

  int i[4] = {[0] = 10, [2] = 100};

  printf("Hello World!\n");
}
