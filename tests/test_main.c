#include <stdio.h>

// Forward declarations of test suites
extern int run_tests(void);

int main(void) {
    printf("Starting Tests...\n");
    int failures = run_tests();
    if (failures > 0) {
        printf("Tests FAILED: %d errors.\n", failures);
        return 1;
    }
    printf("All Tests PASSED.\n");
    return 0;
}
