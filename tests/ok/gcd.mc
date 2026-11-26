int gcd(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

void report(int a, int b) {
    print(gcd(a, b));
}

int main() {
    int n = read();
    while (n > 0) {
        report(read(), read());
        n = n - 1;
    }
    return 0;
}
