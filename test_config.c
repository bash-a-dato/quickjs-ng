/* Simple test to verify CONFIG_DEBUGGER is defined */
#include <stdio.h>

int main() {
#ifdef CONFIG_DEBUGGER
    printf("CONFIG_DEBUGGER is ENABLED\n");
    printf("Debug file manager should be available\n");
    return 0;
#else
    printf("CONFIG_DEBUGGER is DISABLED\n");
    printf("Debug file manager is not available\n");
    return 1;
#endif
}
