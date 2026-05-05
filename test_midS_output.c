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


char* get_valid_string(char* str_val) {
    if ((str_val == NULL)) {
        return "DEFAULT_EMPTY";
    }
    return str_val;
}

int mainLua() {
    void* missing_data = NULL;
    char* actual_data = "REAL_DATA";
    char* result1 = get_valid_string(missing_data);
    char* result2 = get_valid_string(actual_data);
    if (str_eq(result1, "DEFAULT_EMPTY")) {
        if (str_eq(result2, "REAL_DATA")) {
            return 1;
        }
    }
    return 0;
}

int main() {
    mainLua();
    return 0;
}
