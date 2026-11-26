bool is_prime(int n) {
    if (n < 2) return false;
    int d = 2;
    while (d * d <= n) {
        if (n % d == 0) return false;
        d = d + 1;
    }
    return true;
}

int main() {
    int n = 0;
    while (n < 40) {
        if (is_prime(n)) print(n);
        n = n + 1;
    }
    return 0;
}
