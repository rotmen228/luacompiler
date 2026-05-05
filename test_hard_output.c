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

int steps;

int is_prime_helper(int n, int divisor) {
    if ((n <= 1)) {
        return 0;
    }
    if ((divisor == 1)) {
        return 1;
    }
    int rem = (n % divisor);
    if ((rem == 0)) {
        return 0;
    }
    return is_prime_helper(n, (divisor - 1));
}

int is_prime(int n) {
    return is_prime_helper(n, (n - 1));
}

int collatz_steps(int n) {
    steps = 0;
    while ((n > 1)) {
        int rem = (n % 2);
        if ((rem == 0)) {
            n = (n / 2);
        } else {
            n = ((n * 3) + 1);
        }
        steps = (steps + 1);
    }
    return steps;
}

int complex_calculation(int limit) {
    int sum = 0;
    int i = 1;
    while ((i <= limit)) {
        int current_val = i;
        int add_to_sum = 0;
        if ((is_prime(current_val) == 1)) {
            int __tmp_shadow_current_val_0 = (current_val * 10);
            int current_val = __tmp_shadow_current_val_0;
            add_to_sum = current_val;
        } else if (((current_val > 10) && (collatz_steps(current_val) > 15))) {
            add_to_sum = (current_val * 2);
        } else {
            add_to_sum = current_val;
        }
        sum = (sum + add_to_sum);
        i = (i + 1);
    }
    return sum;
}

int mainLua() {
    int target = 20;
    int result = complex_calculation(target);
    printf("%d\n", target);
    printf("%d\n", result);
    return result;
}

int main() {
    void* rot = NULL;
    char* man = concat_strings("hello", " world");
    mainLua();
    return 0;
}
