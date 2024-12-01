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

extern int return_5();

extern int return_7();

int main() {
  int a = return_5();
  int b = return_7();

  return a + b;

  // printf
  
  // return 0;
}