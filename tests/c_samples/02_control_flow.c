// 02_control_flow.c - If/else, while, for, break, continue, short-circuit

int main() {
    int x = 10;
    int sign = 0;

    if (x > 0) {
        sign = 1;
    } else if (x < 0) {
        sign = -1;
    } else {
        sign = 0;
    }

    // While loop with continue and break
    int sum = 0;
    int i = 0;
    while (i < 20) {
        i++;
        if (i % 2 == 1) {
            continue; // Skip odds: 1, 3, 5, 7, 9
        }
        if (i > 10) {
            break; // Stop when i reaches 12
        }
        sum += i; // 2 + 4 + 6 + 8 + 10 = 30
    }

    // For loop with nested loop
    int gridSum = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            gridSum += 1; // 4 * 3 = 12
        }
    }

    // Short-circuit check
    int sideEffect = 0;
    int sc1 = (1 || (sideEffect = 100)); // sideEffect should NOT be modified
    int sc2 = (0 && (sideEffect = 200)); // sideEffect should NOT be modified

    // sign (1) + sum (30) + gridSum (12) + sideEffect (0) = 43
    return sign + sum + gridSum + sideEffect;
}
