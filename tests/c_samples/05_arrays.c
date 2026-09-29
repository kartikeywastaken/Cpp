// 05_arrays.c - Array indexing, pointer decay, passing array to function

int sum_array(int* arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

int main() {
    int nums[5];
    for (int i = 0; i < 5; i++) {
        nums[i] = (i + 1) * 10; // 10, 20, 30, 40, 50
    }

    // Modify using pointer arithmetic
    *(nums + 2) = 35; // nums[2] is now 35

    // sum = 10 + 20 + 35 + 40 + 50 = 155
    int total = sum_array(nums, 5);

    return total;
}
