#ifndef REDUX_CLOCK_H
#define REDUX_CLOCK_H

#include <stdint.h>

uint32_t calibrate_lapic_timer(void);

void init_one_shot_clock(
    uint8_t vector
);

void redux_clock_set_reload(
    uint32_t count
);

uint32_t redux_clock_get_reload(void);

uint32_t redux_clock_get_current_count(void);

uint32_t redux_clock_get_ticks_per_ms(void);

uint32_t redux_clock_ms_to_ticks(
    uint32_t milliseconds
);

void arm_redux_clock(
    uint32_t count
);

uint64_t redux_clock_get_system_ticks(void);

void redux_clock_c_handler(void);

#endif