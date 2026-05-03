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

static char* man;
int steps;
static void* rot;

int is_prime_helper(int n, int divisor) {
    if ((n <= 1)) {
        return 0;
    }
    if ((divisor == 1)) {
        return 1;
    }
    void* rem = (n % divisor);
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
        void* rem = (n % 2);
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
    void* sum = 0;
    void* i = 1;
    while ((i <= limit)) {
        void* current_val = i;
        void* add_to_sum = 0;
        if ((is_prime(current_val) == 1)) {
            void* current_val = (current_val * 10);
            add_to_sum = current_val;
        } else if (((current_val > 10) and (collatz_steps(current_val) > 15))) {
            add_to_sum = (current_val * 2);
        } else {
            add_to_sum = current_val;
        }
        sum = (sum + add_to_sum);
        i = (i + 1);
    }
    return sum;
}

int main() {
    void* target = 20;
    void* result = complex_calculation(target);
    return result;
}

int main() {
    static void* rot = NULL;
    static char* man = concat_strings(hello,  world);
    return main();
    return 0;
}
