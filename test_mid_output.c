#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Lua string concatenation helper
static char* concat_strings(const char* a, const char* b) {
    char* result = (char*)malloc(strlen(a) + strlen(b) + 1);
    strcpy(result, a);
    strcat(result, b);
    return result;
}


int factorial(int n) {
    if ((n <= 1)) {
        return 1;
    } else {
        return (n * factorial((n - 1)));
    }
}

int main() {
    int sum = 0;
    int i = 1;
    while ((i <= 5)) {
        sum = (sum + factorial(i));
        i = (i + 1);
    }
    return sum;
    return 0;
}
