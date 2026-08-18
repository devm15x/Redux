#ifndef REDUX_ACPI_H
#define REDUX_ACPI_H

#include <stdint.h>
#include <stdbool.h>

bool acpi_init(void *rsdp_address,uint64_t hhdm_offset);

bool acpi_pm_timer_available(void);

uint32_t acpi_pm_timer_read(void);

void acpi_pm_wait_ms(uint32_t ms);

uint64_t acpi_pm_timer_frequency(void);

#endif