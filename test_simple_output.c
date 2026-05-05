#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Lua string concatenation helper
static char* concat_strings(const char* a, const char* b) {
    if (!a) a = "nil"; if (!b) b = "nil";
    char* result = (char*)malloc(strlen(a) + strlen(b) + 1);
    strcpy(result, a);
    strcat(result, b);
    return result;
}

// Lua number-to-string coercion helper (for .. with numbers)
static char* num_to_str(double v) {
    char* buf = (char*)malloc(64);
    if (v == (long long)v) snprintf(buf, 64, "%lld", (long long)v);
    else snprintf(buf, 64, "%g", v);
    return buf;
}

// Lua string equality helper (strcmp wrapper)
static int str_eq(const char* a, const char* b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
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
