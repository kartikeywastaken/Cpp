
int add(int a, int b) { return a + b; }

int main() {
  int x = 15;
  int y = 27;
  int sum = add(x, y);

  puts("--- Running My Custom C Program ---");
  print_str("The sum of 15 and 27 is: ");
  print_int(sum);

  return 0;
}