int fib_rec(int n) {
    if (n < 2) return n;
    return fib_rec(n - 1) + fib_rec(n - 2);
}

int fib_iter(int n) {
    int a = 0;
    int b = 1;
    while (n > 0) {
        int next = a + b;
        a = b;
        b = next;
        n = n - 1;
    }
    return a;
}

int main() {
    print(fib_rec(20));
    print(fib_iter(20));
    print(fib_iter(90));
    return 0;
}
