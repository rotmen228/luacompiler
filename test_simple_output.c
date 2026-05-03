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


int main() {
    int a = 10;
    int b = 20;
    int c = 5;
    int result = ((a + b) * c);
    int final_value = (result / 2);
    return final_value;
    return 0;
}
