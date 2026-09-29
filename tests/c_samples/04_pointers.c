// 04_pointers.c - Address-of, dereference, pointer modification, double pointers

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main() {
    int x = 10;
    int y = 20;

    int* px = &x;
    int* py = &y;

    swap(px, py); // Now x = 20, y = 10

    // Double pointer
    int** ppx = &px;
    **ppx = 30; // Now x = 30

    // Pointer arithmetic
    int val = 100;
    int* p = &val;
    int* p2 = p + 0;
    *p2 = *p + 5; // val = 105

    // x (30) + y (10) + val (105) = 145
    return x + y + val;
}
