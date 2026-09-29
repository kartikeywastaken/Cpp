// 03_functions.c - Function declarations, prototypes, recursion, and calls

int factorial(int n);
int fibonacci(int n);
int add3(int a, int b, int c);

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int add3(int a, int b, int c) {
    return a + b + c;
}

int main() {
    int fact5 = factorial(5); // 120
    int fib7 = fibonacci(7);  // 13: 0, 1, 1, 2, 3, 5, 8, 13
    int sum = add3(fact5, fib7, 7); // 120 + 13 + 7 = 140
    return sum;
}
