// #include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// void test() {

//   int a = 5;
//   // ++a;
//   // ++a;
//   // ++a;

//   // int a = 5;
//   // for (int i = 0; i < 10; i++) {
//   //   a += i;
//   // }

//   printf("a: %d\n", a);
// }

int return_5() { return 5; }

int return_7() { return 7; }

int main() {
  int a = return_5();
  int b = return_7();

  return a + b;

  // return 0;
}