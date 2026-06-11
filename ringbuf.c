#include <stdio.h>

int main(void) {
    printf("Hello, C99 World!\n");
    _Static_assert(sizeof(int) >= 4, "Error: Standard integers must be at least 32-bit");
    return 0;
}
