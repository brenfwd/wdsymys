#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  int a = 5;

  for (int i = 0; i < 2; i++) {
    ++a;
    ++a;
    ++a;
  }

  return a; // 11
}
