#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc) {
    int a = 0;
    int b = 0;

    if (argc > 0) {
        a = a + 1;
        b = b + 3;
    } else {
        a = a + 2;
        b = b + 5;
    }

    return a + b;
}
