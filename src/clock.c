#include <stdint.h>

#include "apic.h"
#include "acpi.h"
#include "clock.h"

#define APIC_EOI_REGISTER   0x0B0

#define APIC_LVT_TIMER      0x320
#define APIC_TICR           0x380
#define APIC_TCCR           0x390
#define APIC_TDCR           0x3E0

#define APIC_TIMER_MASKED   (1u << 16)

volatile uint64_t redux_system_ticks = 0;

static uint32_t redux_clock_reload = 0;
static uint32_t apic_ticks_per_ms = 0;

extern void apic_timer_isr_stub(void);

uint32_t calibrate_lapic_timer(void)
{
    lapic_write(
        APIC_TDCR,
        0x3
    );

    lapic_write(
        APIC_LVT_TIMER,
        APIC_TIMER_MASKED
    );

    lapic_write(
        APIC_TICR,
        0xFFFFFFFFu
    );

    acpi_pm_wait_ms(10);

    uint32_t remaining =
        lapic_read(
            APIC_TCCR
        );

    lapic_write(
        APIC_TICR,
        0
    );

    uint32_t elapsed =
        0xFFFFFFFFu -
        remaining;

    if (elapsed == 0)
    {
        return 0;
    }

    apic_ticks_per_ms =
        elapsed / 10;

    return
        apic_ticks_per_ms;
}

void init_one_shot_clock(
    uint8_t vector
)
{
    redux_system_ticks = 0;

    lapic_write(
        APIC_TDCR,
        0x3
    );

    lapic_write(
        APIC_LVT_TIMER,
        (uint32_t)vector
    );
}

void redux_clock_set_reload(
    uint32_t count
)
{
    redux_clock_reload =
        count;
}

uint32_t redux_clock_get_reload(void)
{
    return redux_clock_reload;
}

uint32_t redux_clock_get_current_count(void)
{
    return lapic_read(
        APIC_TCCR
    );
}

uint32_t redux_clock_get_ticks_per_ms(void)
{
    return apic_ticks_per_ms;
}

uint32_t redux_clock_ms_to_ticks(
    uint32_t milliseconds
)
{
    if (apic_ticks_per_ms == 0)
    {
        return 0;
    }

    uint64_t count =
        (uint64_t)apic_ticks_per_ms *
        (uint64_t)milliseconds;

    if (count > 0xFFFFFFFFULL)
    {
        count =
            0xFFFFFFFFULL;
    }

    return (uint32_t)count;
}

void arm_redux_clock(
    uint32_t count
)
{
    lapic_write(
        APIC_TICR,
        count
    );
}

uint64_t redux_clock_get_system_ticks(void)
{
    return redux_system_ticks;
}

void redux_clock_c_handler(void)
{
    redux_system_ticks++;

    lapic_write(
        APIC_EOI_REGISTER,
        0
    );

    if (redux_clock_reload != 0)
    {
        lapic_write(
            APIC_TICR,
            redux_clock_reload
        );
    }
}