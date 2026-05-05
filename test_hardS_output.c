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


char* check_status(int val) {
    if ((val == NULL)) {
        return "EMPTY";
    }
    if (str_eq(val, "ERROR")) {
        return "FAILED";
    }
    return concat_strings("OK_", num_to_str(val));
}

void* build_sequence(char* start_str, int max_iterations) {
    void* result = start_str;
    int i = 0;
    while ((i < max_iterations)) {
        int __tmp_shadow_i_0 = (i + 1);
        int i = __tmp_shadow_i_0;
        char* suffix = "";
        int rem = (i % 2);
        if ((rem == 0)) {
            suffix = "_E";
        } else {
            suffix = "_O";
        }
        result = concat_strings(concat_strings(result, suffix), num_to_str(i));
        void* temp = NULL;
        if (((i > 5) && (temp == NULL))) {
            result = concat_strings(result, "-MID");
        }
        if (str_eq(result, "START_O1_E2_O3_E4_O5_E6-MID")) {
            result = concat_strings(result, "-STOP");
            i = max_iterations;
        }
    }
    return result;
}

void* mainLua() {
    void* uninitialized = NULL;
    char* status1 = check_status(uninitialized);
    char* status2 = check_status(100);
    void* sequence = build_sequence("START", 8);
    void* final_res = concat_strings(concat_strings(concat_strings(concat_strings(status1, "|"), status2), "|"), sequence);
    return final_res;
}

int main() {
    mainLua();
    return 0;
}
