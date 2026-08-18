#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "acpi.h"

#define ACPI_PM_TIMER_FREQUENCY 3579545ULL

#define ACPI_ADDRESS_SPACE_MEMORY 0
#define ACPI_ADDRESS_SPACE_IO     1

typedef struct
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_v1_t;

typedef struct
{
    rsdp_v1_t first_part;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) rsdp_v2_t;

typedef struct
{
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed)) acpi_gas_t;

typedef struct
{
    acpi_sdt_header_t header;

    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t reserved;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;

    uint32_t smi_command_port;

    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;

    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;

    uint32_t gpe0_block;
    uint32_t gpe1_block;

    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;

    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;

    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;

    uint16_t flush_size;
    uint16_t flush_stride;

    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;

    uint16_t boot_architecture_flags;
    uint8_t reserved2;
    uint32_t flags;

    acpi_gas_t reset_reg;

    uint8_t reset_value;

    uint16_t arm_boot_arch;

    uint8_t minor_version;

    uint64_t x_firmware_control;
    uint64_t x_dsdt;

    acpi_gas_t x_pm1a_event_block;
    acpi_gas_t x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block;
    acpi_gas_t x_pm1b_control_block;
    acpi_gas_t x_pm2_control_block;
    acpi_gas_t x_pm_timer_block;
} __attribute__((packed)) fadt_t;

static uint64_t acpi_hhdm_offset = 0;

static bool pm_timer_present = false;
static bool pm_timer_is_32bit = false;
static bool pm_timer_is_io = false;

static uint16_t pm_timer_port = 0;

static volatile uint32_t *pm_timer_mmio = NULL;

static inline void debug_putc(
    char c
)
{
    __asm__ volatile(
        "outb %0, $0xE9"
        :
        : "a"(c)
    );
}

static void debug_print(
    const char *text
)
{
    while (*text)
    {
        debug_putc(
            *text++
        );
    }

    debug_putc('\n');
}

static inline uint32_t inl(
    uint16_t port
)
{
    uint32_t value;

    __asm__ volatile(
        "inl %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static void *physical_to_virtual(
    uint64_t physical_address
)
{
    return (void *)(
        (uintptr_t)physical_address +
        acpi_hhdm_offset
    );
}

static bool signature_equals(
    const char *a,
    const char *b
)
{
    return
        a[0] == b[0] &&
        a[1] == b[1] &&
        a[2] == b[2] &&
        a[3] == b[3];
}

static bool signature8_equals(
    const char *a,
    const char *b
)
{
    for (uint8_t i = 0;
         i < 8;
         i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return true;
}

static bool checksum_ok(
    const void *address,
    uint32_t length
)
{
    const uint8_t *bytes =
        (const uint8_t *)address;

    uint8_t sum = 0;

    for (uint32_t i = 0;
         i < length;
         i++)
    {
        sum =
            (uint8_t)(
                sum + bytes[i]
            );
    }

    return sum == 0;
}

static fadt_t *validate_fadt(
    acpi_sdt_header_t *header
)
{
    if (header == NULL)
    {
        return NULL;
    }

    if (!signature_equals(
        header->signature,
        "FACP"
    ))
    {
        return NULL;
    }

    debug_print(
        "ACPI: FADT signature found"
    );

    if (header->length <
        sizeof(acpi_sdt_header_t))
    {
        debug_print(
            "ACPI FAIL: bad FADT length"
        );

        return NULL;
    }

    if (!checksum_ok(
        header,
        header->length
    ))
    {
        debug_print(
            "ACPI FAIL: bad FADT checksum"
        );

        return NULL;
    }

    return (fadt_t *)header;
}

static fadt_t *find_fadt_rsdt(
    acpi_sdt_header_t *rsdt
)
{
    if (rsdt == NULL)
    {
        debug_print(
            "ACPI FAIL: RSDT null"
        );

        return NULL;
    }

    if (!signature_equals(
        rsdt->signature,
        "RSDT"
    ))
    {
        debug_print(
            "ACPI FAIL: bad RSDT signature"
        );

        return NULL;
    }

    if (rsdt->length <
        sizeof(acpi_sdt_header_t))
    {
        debug_print(
            "ACPI FAIL: bad RSDT length"
        );

        return NULL;
    }

    if (!checksum_ok(
        rsdt,
        rsdt->length
    ))
    {
        debug_print(
            "ACPI FAIL: bad RSDT checksum"
        );

        return NULL;
    }

    debug_print(
        "ACPI: RSDT valid"
    );

    uint32_t entry_count =
        (
            rsdt->length -
            sizeof(acpi_sdt_header_t)
        ) /
        sizeof(uint32_t);

    uint32_t *entries =
        (uint32_t *)(
            (uintptr_t)rsdt +
            sizeof(acpi_sdt_header_t)
        );

    for (uint32_t i = 0;
         i < entry_count;
         i++)
    {
        if (entries[i] == 0)
        {
            continue;
        }

        acpi_sdt_header_t *header =
            (acpi_sdt_header_t *)
            physical_to_virtual(
                entries[i]
            );

        fadt_t *fadt =
            validate_fadt(
                header
            );

        if (fadt != NULL)
        {
            return fadt;
        }
    }

    debug_print(
        "ACPI FAIL: FADT not found in RSDT"
    );

    return NULL;
}

static fadt_t *find_fadt_xsdt(
    acpi_sdt_header_t *xsdt
)
{
    if (xsdt == NULL)
    {
        debug_print(
            "ACPI FAIL: XSDT null"
        );

        return NULL;
    }

    if (!signature_equals(
        xsdt->signature,
        "XSDT"
    ))
    {
        debug_print(
            "ACPI FAIL: bad XSDT signature"
        );

        return NULL;
    }

    if (xsdt->length <
        sizeof(acpi_sdt_header_t))
    {
        debug_print(
            "ACPI FAIL: bad XSDT length"
        );

        return NULL;
    }

    if (!checksum_ok(
        xsdt,
        xsdt->length
    ))
    {
        debug_print(
            "ACPI FAIL: bad XSDT checksum"
        );

        return NULL;
    }

    debug_print(
        "ACPI: XSDT valid"
    );

    uint32_t entry_count =
        (
            xsdt->length -
            sizeof(acpi_sdt_header_t)
        ) /
        sizeof(uint64_t);

    uint64_t *entries =
        (uint64_t *)(
            (uintptr_t)xsdt +
            sizeof(acpi_sdt_header_t)
        );

    for (uint32_t i = 0;
         i < entry_count;
         i++)
    {
        if (entries[i] == 0)
        {
            continue;
        }

        acpi_sdt_header_t *header =
            (acpi_sdt_header_t *)
            physical_to_virtual(
                entries[i]
            );

        fadt_t *fadt =
            validate_fadt(
                header
            );

        if (fadt != NULL)
        {
            return fadt;
        }
    }

    debug_print(
        "ACPI FAIL: FADT not found in XSDT"
    );

    return NULL;
}

bool acpi_init(
    void *rsdp_address,
    uint64_t hhdm_offset
)
{
    acpi_hhdm_offset =
        hhdm_offset;

    pm_timer_present = false;
    pm_timer_is_32bit = false;
    pm_timer_is_io = false;

    pm_timer_port = 0;
    pm_timer_mmio = NULL;

    debug_print(
        "ACPI: init start"
    );

    if (rsdp_address == NULL)
    {
        debug_print(
            "ACPI FAIL: RSDP null"
        );

        return false;
    }

    rsdp_v1_t *rsdp1 =
        (rsdp_v1_t *)
        rsdp_address;

    debug_print(
        "ACPI: RSDP pointer valid"
    );

    if (!signature8_equals(
        rsdp1->signature,
        "RSD PTR "
    ))
    {
        debug_print(
            "ACPI FAIL: bad RSDP signature"
        );

        return false;
    }

    debug_print(
        "ACPI: RSDP signature OK"
    );

    if (!checksum_ok(
        rsdp1,
        sizeof(rsdp_v1_t)
    ))
    {
        debug_print(
            "ACPI FAIL: bad RSDP checksum"
        );

        return false;
    }

    debug_print(
        "ACPI: RSDP checksum OK"
    );

    fadt_t *fadt = NULL;

    if (rsdp1->revision >= 2)
    {
        debug_print(
            "ACPI: ACPI 2.0+ detected"
        );

        rsdp_v2_t *rsdp2 =
            (rsdp_v2_t *)
            rsdp_address;

        if (rsdp2->length <
            sizeof(rsdp_v2_t))
        {
            debug_print(
                "ACPI FAIL: bad extended RSDP length"
            );

            return false;
        }

        if (!checksum_ok(
            rsdp2,
            rsdp2->length
        ))
        {
            debug_print(
                "ACPI FAIL: bad extended RSDP checksum"
            );

            return false;
        }

        if (rsdp2->xsdt_address != 0)
        {
            debug_print(
                "ACPI: using XSDT"
            );

            acpi_sdt_header_t *xsdt =
                (acpi_sdt_header_t *)
                physical_to_virtual(
                    rsdp2->xsdt_address
                );

            fadt =
                find_fadt_xsdt(
                    xsdt
                );
        }
        else
        {
            debug_print(
                "ACPI: XSDT missing, falling back to RSDT"
            );
        }
    }
    else
    {
        debug_print(
            "ACPI: ACPI 1.0 detected"
        );
    }

    if (fadt == NULL)
    {
        if (rsdp1->rsdt_address == 0)
        {
            debug_print(
                "ACPI FAIL: no RSDT"
            );

            return false;
        }

        debug_print(
            "ACPI: using RSDT"
        );

        acpi_sdt_header_t *rsdt =
            (acpi_sdt_header_t *)
            physical_to_virtual(
                rsdp1->rsdt_address
            );

        fadt =
            find_fadt_rsdt(
                rsdt
            );
    }

    if (fadt == NULL)
    {
        debug_print(
            "ACPI FAIL: could not get FADT"
        );

        return false;
    }

    debug_print(
        "ACPI: FADT valid"
    );

    if (
        fadt->header.length <=
        offsetof(
            fadt_t,
            pm_timer_length
        )
    )
    {
        debug_print(
            "ACPI FAIL: FADT too short for PM timer"
        );

        return false;
    }

    if (fadt->pm_timer_length != 4)
    {
        debug_print(
            "ACPI FAIL: PM timer length != 4"
        );

        return false;
    }

    debug_print(
        "ACPI: PM timer length OK"
    );

    if (
        fadt->header.length >=
        offsetof(
            fadt_t,
            flags
        ) +
        sizeof(uint32_t)
    )
    {
        pm_timer_is_32bit =
            (fadt->flags & (1u << 8))
            != 0;
    }
    else
    {
        pm_timer_is_32bit = false;
    }

    size_t x_pm_timer_end =
        offsetof(
            fadt_t,
            x_pm_timer_block
        ) +
        sizeof(acpi_gas_t);

    if (
        fadt->header.length >=
        x_pm_timer_end &&
        fadt->x_pm_timer_block.address != 0
    )
    {
        if (
            fadt->x_pm_timer_block.address_space ==
            ACPI_ADDRESS_SPACE_IO
        )
        {
            pm_timer_is_io = true;

            pm_timer_port =
                (uint16_t)
                fadt->x_pm_timer_block.address;

            debug_print(
                "ACPI: using X_PM_TMR_BLK IO"
            );
        }
        else if (
            fadt->x_pm_timer_block.address_space ==
            ACPI_ADDRESS_SPACE_MEMORY
        )
        {
            pm_timer_is_io = false;

            pm_timer_mmio =
                (volatile uint32_t *)
                physical_to_virtual(
                    fadt->x_pm_timer_block.address
                );

            debug_print(
                "ACPI: using X_PM_TMR_BLK MMIO"
            );
        }
        else
        {
            debug_print(
                "ACPI FAIL: unsupported PM timer address space"
            );

            return false;
        }
    }
    else
    {
        if (fadt->pm_timer_block == 0)
        {
            debug_print(
                "ACPI FAIL: legacy PM timer address zero"
            );

            return false;
        }

        pm_timer_is_io = true;

        pm_timer_port =
            (uint16_t)
            fadt->pm_timer_block;

        debug_print(
            "ACPI: using legacy PM_TMR_BLK"
        );
    }

    if (
        pm_timer_is_io &&
        pm_timer_port == 0
    )
    {
        debug_print(
            "ACPI FAIL: PM timer port zero"
        );

        return false;
    }

    if (
        !pm_timer_is_io &&
        pm_timer_mmio == NULL
    )
    {
        debug_print(
            "ACPI FAIL: PM timer MMIO null"
        );

        return false;
    }

    pm_timer_present = true;

    if (pm_timer_is_32bit)
    {
        debug_print(
            "ACPI: PM timer is 32-bit"
        );
    }
    else
    {
        debug_print(
            "ACPI: PM timer is 24-bit"
        );
    }

    debug_print(
        "ACPI: initialization complete"
    );

    return true;
}

bool acpi_pm_timer_available(void)
{
    return pm_timer_present;
}

uint32_t acpi_pm_timer_read(void)
{
    if (!pm_timer_present)
    {
        return 0;
    }

    uint32_t value;

    if (pm_timer_is_io)
    {
        value =
            inl(
                pm_timer_port
            );
    }
    else
    {
        value =
            *pm_timer_mmio;
    }

    if (!pm_timer_is_32bit)
    {
        value &=
            0x00FFFFFFu;
    }

    return value;
}

void acpi_pm_wait_ms(
    uint32_t ms
)
{
    if (!pm_timer_present)
    {
        return;
    }

    uint64_t ticks_remaining =
        (
            ACPI_PM_TIMER_FREQUENCY *
            (uint64_t)ms
        ) /
        1000ULL;

    uint32_t mask =
        pm_timer_is_32bit
        ? 0xFFFFFFFFu
        : 0x00FFFFFFu;

    uint64_t safe_chunk =
        pm_timer_is_32bit
        ? 0x7FFFFFFFULL
        : 0x007FFFFFULL;

    while (ticks_remaining != 0)
    {
        uint32_t target =
            ticks_remaining >
            safe_chunk
            ? (uint32_t)safe_chunk
            : (uint32_t)ticks_remaining;

        uint32_t start =
            acpi_pm_timer_read();

        while (1)
        {
            uint32_t now =
                acpi_pm_timer_read();

            uint32_t elapsed =
                (now - start) &
                mask;

            if (elapsed >= target)
            {
                break;
            }

            __asm__ volatile(
                "pause"
            );
        }

        ticks_remaining -=
            target;
    }
}

uint64_t acpi_pm_timer_frequency(void)
{
    return ACPI_PM_TIMER_FREQUENCY;
}