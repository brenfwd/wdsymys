#include <stdlib.h>

struct ListNode {
    struct ListNode* next;
    struct ListNode* prev;
    int val;
};


void generate_spills() {
    // Declare many variables
    int a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z;
    // Arrays of large size
    struct ListNode head;
    head.val = 0;
    head.next = 0;
    head.prev = 0;

    struct ListNode* curr = &head;
    
    // Fill arrays with some values
    for (int i = 0; i < 1000; ++i) {
        struct ListNode* n = malloc(sizeof(struct ListNode));
        n->val = i;
        curr->next = n;
        n->prev = curr;
        n->next = 0;
    }
    
    // Perform some operations
    a = b = c = d = e = f = g = h = i = j = k = l = m = n = o = p = q = r = s = t = u = v = w = x = y = z = 1;
    int pain = a * b * c * d * e * f * g * h * i * j * k * l * m * n * o * p * q * r * s * t * u * v *w * x * y * z;
    int sum = 0;
    curr = &head;
    for (int i = 0; i < 1000; ++i) {
        sum += curr->val;
        curr = curr->next;
    }

    // Print out a value to prevent optimization
}

int main() {
    generate_spills();
    return 0;
}
