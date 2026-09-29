// 08_errors.c - Semantic errors for compiler diagnostic validation

int testErrors() {
    int x = 10;
    int x = 20; // Error: redeclaration

    y = 50;     // Error: undeclared identifier

    10 = x;     // Error: lvalue required

    break;      // Error: break outside loop

    return 0;
}
