// 01_arithmetic.c - Arithmetic, bitwise, and assignment operations

int main() {
    int a = 10;
    int b = 3;

    int sum = a + b;       // 13
    int diff = a - b;      // 7
    int prod = a * b;      // 30
    int quot = a / b;      // 3
    int rem = a % b;       // 1

    int bitwise = (a & b) | (a ^ b); // (10 & 3) | (10 ^ 3) = 2 | 9 = 11
    int shifted = (1 << 4) >> 2;     // 16 >> 2 = 4

    int c = 5;
    c += 10; // 15
    c *= 2;  // 30
    c -= 5;  // 25
    c /= 5;  // 5

    int d = c++; // d = 5, c = 6
    int e = ++c; // c = 7, e = 7

    // Result calculation
    int result = sum + diff + prod + quot + rem + bitwise + shifted + c + d + e;
    // 13 + 7 + 30 + 3 + 1 + 11 + 4 + 7 + 5 + 7 = 88
    return result;
}
