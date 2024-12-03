#include <stdint.h>
#include <stdio.h>

int register_pressure_test() {
    // Declare over 100 int32_t variables
    int32_t a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5;
    int32_t a6 = 6, a7 = 7, a8 = 8, a9 = 9, a10 = 10;
    int32_t a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15;
    int32_t a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20;
    int32_t a21 = 21, a22 = 22, a23 = 23, a24 = 24, a25 = 25;
    int32_t a26 = 26, a27 = 27, a28 = 28, a29 = 29, a30 = 30;
    int32_t a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35;
    int32_t a36 = 36, a37 = 37, a38 = 38, a39 = 39, a40 = 40;
    int32_t a41 = 41, a42 = 42, a43 = 43, a44 = 44, a45 = 45;
    int32_t a46 = 46, a47 = 47, a48 = 48, a49 = 49, a50 = 50;
    int32_t a51 = 51, a52 = 52, a53 = 53, a54 = 54, a55 = 55;
    int32_t a56 = 56, a57 = 57, a58 = 58, a59 = 59, a60 = 60;
    int32_t a61 = 61, a62 = 62, a63 = 63, a64 = 64, a65 = 65;
    int32_t a66 = 66, a67 = 67, a68 = 68, a69 = 69, a70 = 70;
    int32_t a71 = 71, a72 = 72, a73 = 73, a74 = 74, a75 = 75;
    int32_t a76 = 76, a77 = 77, a78 = 78, a79 = 79, a80 = 80;
    int32_t a81 = 81, a82 = 82, a83 = 83, a84 = 84, a85 = 85;
    int32_t a86 = 86, a87 = 87, a88 = 88, a89 = 89, a90 = 90;
    int32_t a91 = 91, a92 = 92, a93 = 93, a94 = 94, a95 = 95;
    int32_t a96 = 96, a97 = 97, a98 = 98, a99 = 99, a100 = 100;

    int32_t result = 0;

    // Complex nested loops with if/else-if/else blocks
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            if (i % 3 == 0) {
                a1 += a2 + a3 - a4;
                a5 *= (a6 & a7);
                result ^= a8 + a9;
            } else if (i % 3 == 1) {
                a10 ^= a11 | a12;
                a13 += a14 % (a15 + 1);
                a16 -= a17 * a18;
            } else {
                a19 &= a20 ^ a21;
                a22 -= a23 + a24;
                result |= (a25 << 1) + (a26 >> 1);
            }

            if (j % 2 == 0) {
                a27 += (a28 * a29) ^ a30;
                a31 &= (a32 - a33);
                result += a34 + a35;
            } else if (j % 5 == 0) {
                a36 ^= (a37 + a38) | a39;
                a40 += (a41 - a42) % (a43 + 1);
                result -= a44 + a45;
            } else {
                a46 |= a47 & a48;
                a49 -= a50 + a51;
                result ^= a52 | a53;
            }

            for (int k = 0; k < 5; k++) {
                if (k % 2 == 0) {
                    a54 += (a55 * a56) ^ (a57 - a58);
                    a59 &= (a60 | a61);
                    result += a62 ^ a63;
                } else {
                    a64 ^= (a65 - a66) + a67;
                    a68 -= (a69 * a70) % (a71 + 1);
                    result |= a72 & a73;
                }
            }
        }
    }

    // Additional loop with conditions
    for (int i = 0; i < 20; i++) {
        if (i < 10) {
            a74 += a75 - a76;
            a77 ^= (a78 & a79);
            result ^= a80 + a81;
        } else {
            a82 -= (a83 * a84) + a85;
            a86 |= (a87 ^ a88);
            result += a89 & a90;
        }
    }

    // Final computation using all variables
    result ^= a91 + a92 ^ a93;
    result ^= a94 + a95 * a96;
    result ^= a97 + a98 | a99;
    result ^= a100;

    return result;
}

int main() {
    int final_result = register_pressure_test();
    printf("Final Result: %d\n", final_result); // 14684
    return final_result; // 92
}
