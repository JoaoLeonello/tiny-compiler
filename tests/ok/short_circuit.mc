bool loud(int v, bool result) {
    print(v);
    return result;
}

int main() {
    if (loud(1, false) && loud(2, true)) print(100);
    if (loud(3, true) || loud(4, true)) print(200);
    bool b = loud(5, false) || loud(6, true);
    if (b) print(300);
    return 0;
}
