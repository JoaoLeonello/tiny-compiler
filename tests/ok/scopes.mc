int x(int x) {
    return x * 2;
}

int main() {
    int a = 1;
    {
        int a = 2;
        print(a);
        {
            int a = a + 10;
            print(a);
        }
        print(a);
    }
    print(a);
    print(x(a + 20));
    return 0;
}
