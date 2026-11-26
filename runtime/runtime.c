#include <stdio.h>
#include <stdlib.h>

void mc_print(long value) {
    printf("%ld\n", value);
}

long mc_read(void) {
    long value;
    if (scanf("%ld", &value) != 1) {
        fprintf(stderr, "runtime error: expected an integer on standard input\n");
        exit(1);
    }
    return value;
}
