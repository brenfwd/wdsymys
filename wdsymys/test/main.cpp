#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

void test() {
  int a = 5;
  for (int i = 0; i < 10; i++) {
    a += i;
  }

  std::cout << a << '\n';
}

int main()
{
    int a = 5;
    return 0;
}