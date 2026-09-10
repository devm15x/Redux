#include <stdint.h>

void usermode_test(void);

extern uint64_t jump_usermode(
    uint64_t entry,
    uint64_t user_stack,
    uint64_t api
);
