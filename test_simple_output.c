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

static int result;
static int a;
static int b;
static int c;
static int final_value;

int main() {
    static int a = 10;
    static int b = 20;
    static int c = 5;
    static int result = ((a + b) * c);
    static int final_value = (result / 2);
    return final_value;
    return 0;
}
