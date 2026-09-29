// 06_structs.c - Struct definition, member access (. and ->), struct pointers

struct Vector3 {
    int x;
    int y;
    int z;
};

void setVector(struct Vector3* vec, int x, int y, int z) {
    vec->x = x;
    vec->y = y;
    vec->z = z;
}

int vectorLengthSq(struct Vector3* vec) {
    return vec->x * vec->x + vec->y * vec->y + vec->z * vec->z;
}

int main() {
    struct Vector3 v;
    v.x = 0;
    v.y = 0;
    v.z = 0;

    struct Vector3* pv = &v;
    setVector(pv, 3, 4, 5); // 3^2 + 4^2 + 5^2 = 9 + 16 + 25 = 50

    int lenSq = vectorLengthSq(&v);

    // Direct struct member modification
    v.x += 1; // v.x is now 4; 4*4 + 4*4 + 5*5 = 16 + 16 + 25 = 57
    int lenSq2 = vectorLengthSq(&v);

    // 50 + 57 = 107
    return lenSq + lenSq2;
}
