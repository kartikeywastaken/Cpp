// 07_sort.c - Bubble sort algorithm on array

void bubbleSort(int* a, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (a[j] > a[j + 1]) {
                int temp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = temp;
            }
        }
    }
}

int isSorted(int* a, int n) {
    for (int i = 0; i < n - 1; i++) {
        if (a[i] > a[i + 1]) {
            return 0; // Not sorted
        }
    }
    return 1; // Sorted
}

int main() {
    int arr[8];
    arr[0] = 64;
    arr[1] = 34;
    arr[2] = 25;
    arr[3] = 12;
    arr[4] = 22;
    arr[5] = 11;
    arr[6] = 90;
    arr[7] = 1;

    bubbleSort(arr, 8);

    int sorted = isSorted(arr, 8);
    // arr should be: 1, 11, 12, 22, 25, 34, 64, 90
    // sorted should be 1
    // arr[0] + arr[7] = 1 + 90 = 91

    if (sorted) {
        return arr[0] + arr[7]; // 91
    }

    return 0;
}
