// #include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void test() {

  int a = 5;
  ++a;
  ++a;
  ++a;

  // int a = 5;
  // for (int i = 0; i < 10; i++) {
  //   a += i;
  // }

  printf("a: %d\n", a);
}

int main() {
  int a = 5;
  return 0;
}