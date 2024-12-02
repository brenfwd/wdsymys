#include <stdint.h>

int main() {
    //__int32_t a = 1;
    int16_t b = 2;
    int8_t c = 3;
    int8_t d = 4;
    //a = a * 2;
    b = (int16_t) ((int16_t) b + (int16_t) 2);
    c = (int8_t) c + (int8_t) d;
    return 0;
}