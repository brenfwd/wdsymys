#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int return_5() { return 5; }

int return_7() { return 7; }

int main() {
  int a = return_5();
  int b = return_7();

  int c = a + b;

  return a + b;
}
