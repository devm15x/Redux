#include <stdint.h>
#include <stdbool.h>

#include "cpuid.h"
#include <stddef.h>
/* ============================================================
 * CPUID
 * ============================================================
 */



/* ============================================================
 * IA32_APIC_BASE MSR
 * ============================================================
 */

#define IA32_APIC_BASE_MSR        0x1B

#define IA32_APIC_BASE_MSR_BSP    (1ULL << 8)
#define IA32_APIC_BASE_MSR_X2APIC (1ULL << 10)
#define IA32_APIC_BASE_MSR_ENABLE (1ULL << 11)

/*
 * Physical address portion of IA32_APIC_BASE.
 *
 * Bits 0-11 contain flags/reserved bits.
 */
#define IA32_APIC_BASE_ADDRESS_MASK \
    0x000FFFFFFFFFF000ULL


/* ============================================================
 * Local APIC registers
 * ============================================================
 */

#define LAPIC_REG_EOI             0x0B0
#define LAPIC_REG_SVR             0x0F0

#define LAPIC_REG_LVT_TIMER       0x320
#define LAPIC_REG_INITIAL_COUNT   0x380
#define LAPIC_REG_CURRENT_COUNT   0x390
#define LAPIC_REG_DIVIDE_CONFIG   0x3E0

#define LAPIC_SVR_ENABLE          (1u << 8)

#define LAPIC_SPURIOUS_VECTOR     0xFF


/* ============================================================
 * External low-level CPU functions
 * ============================================================
 */

extern void cpuGetMSR(
    uint32_t msr,
    uint32_t *eax,
    uint32_t *edx
);

extern void cpuSetMSR(
    uint32_t msr,
    uint32_t eax,
    uint32_t edx
);

extern void cpuid(
    uint32_t leaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx
);


/* ============================================================
 * Local APIC state
 * ============================================================
 */

/*
 * IMPORTANT:
 *
 * This must contain a VIRTUAL address through which the LAPIC's
 * physical MMIO page can actually be accessed.
 *
 * If your kernel identity-maps the LAPIC physical address,
 * physical == virtual and this works directly.
 *
 * If not, map the physical LAPIC page first and put the resulting
 * virtual address here.
 */
static volatile uint8_t *lapic_base = NULL;


/* ============================================================
 * APIC detection
 * ============================================================
 */

bool check_apic(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    cpuid(
        1,
        &eax,
        &ebx,
        &ecx,
        &edx
    );

    return
        (edx & CPUID_FEAT_EDX_APIC) != 0;
}


/* ============================================================
 * IA32_APIC_BASE access
 * ============================================================
 */

static uint64_t cpu_get_apic_base_msr(void)
{
    uint32_t eax;
    uint32_t edx;

    cpuGetMSR(
        IA32_APIC_BASE_MSR,
        &eax,
        &edx
    );

    return
        ((uint64_t)edx << 32) |
        (uint64_t)eax;
}

uintptr_t cpu_get_apic_base(void)
{
    uint64_t value =
        cpu_get_apic_base_msr();

    return (uintptr_t)(
        value &
        IA32_APIC_BASE_ADDRESS_MASK
    );
}


/*
 * Change the physical LAPIC base while preserving the control
 * bits already contained in IA32_APIC_BASE.
 *
 * You probably do NOT need to call this during normal startup.
 */
void cpu_set_apic_base(
    uintptr_t apic
)
{
    uint64_t old_value =
        cpu_get_apic_base_msr();

    uint64_t new_base =
        ((uint64_t)apic) &
        IA32_APIC_BASE_ADDRESS_MASK;

    /*
     * Remove only the old address bits.
     *
     * Preserve things such as:
     *   - BSP flag
     *   - x2APIC mode flag
     *   - APIC global enable
     */
    uint64_t new_value =
        old_value &
        ~IA32_APIC_BASE_ADDRESS_MASK;

    new_value |= new_base;

    /*
     * Make sure the Local APIC itself is globally enabled.
     */
    new_value |=
        IA32_APIC_BASE_MSR_ENABLE;

    cpuSetMSR(
        IA32_APIC_BASE_MSR,
        (uint32_t)(
            new_value &
            0xFFFFFFFFULL
        ),
        (uint32_t)(
            new_value >> 32
        )
    );
}


/* ============================================================
 * Local APIC MMIO
 * ============================================================
 */

uint32_t lapic_read(
    uint32_t offset
)
{
    if (lapic_base == NULL)
    {
        return 0;
    }

    volatile uint32_t *reg =
        (volatile uint32_t *)(
            lapic_base +
            offset
        );

    return *reg;
}

void lapic_write(
    uint32_t offset,
    uint32_t value
)
{
    if (lapic_base == NULL)
    {
        return;
    }

    volatile uint32_t *reg =
        (volatile uint32_t *)(
            lapic_base +
            offset
        );

    *reg = value;

    /*
     * Read-back helps ensure the MMIO write has reached
     * the APIC before execution continues.
     */
    (void)lapic_read(
        LAPIC_REG_SVR
    );
}

void apic_set_virtual_base(
    uintptr_t virtual_base
)
{
    lapic_base =
        (volatile uint8_t *)virtual_base;
}


/* ============================================================
 * Local APIC initialization
 * ============================================================
 */

bool enable_apic(void)
{
    if (!check_apic())
    {
        return false;
    }

    uint64_t base_msr =
        cpu_get_apic_base_msr();

    /*
     * This driver uses xAPIC MMIO.
     *
     * If x2APIC is already enabled, registers are accessed
     * through MSRs instead, so do not blindly use MMIO.
     */
    if (base_msr &
        IA32_APIC_BASE_MSR_X2APIC)
    {
        return false;
    }

    /*
     * Make sure the APIC global enable bit is set while
     * preserving everything else in the MSR.
     */
    if (!(base_msr &
          IA32_APIC_BASE_MSR_ENABLE))
    {
        base_msr |=
            IA32_APIC_BASE_MSR_ENABLE;

        cpuSetMSR(
            IA32_APIC_BASE_MSR,
            (uint32_t)base_msr,
            (uint32_t)(
                base_msr >> 32
            )
        );
    }

    uintptr_t physical_base =
        cpu_get_apic_base();

    /*
     * TEMPORARY ASSUMPTION:
     *
     * Redux currently needs the LAPIC physical page to be
     * identity-mapped.
     *
     * Once you have your own paging API, replace this with
     * something like:
     *
     * lapic_base =
     *     map_mmio_page(physical_base);
     */


    /*
     * Enable the Local APIC in software and use vector 0xFF
     * for spurious interrupts.
     */
    uint32_t svr =
        lapic_read(
            LAPIC_REG_SVR
        );

    svr |=
        LAPIC_SVR_ENABLE;

    svr &= ~0xFFu;

    svr |=
        LAPIC_SPURIOUS_VECTOR;

    lapic_write(
        LAPIC_REG_SVR,
        svr
    );

    return true;
}


/* ============================================================
 * End Of Interrupt
 * ============================================================
 */

void apic_eoi(void)
{
    lapic_write(
        LAPIC_REG_EOI,
        0
    );
}


/* ============================================================
 * IOAPIC MMIO
 * ============================================================
 */

uint32_t cpuReadIoApic(
    void *ioapicaddr,
    uint32_t reg
)
{
    if (ioapicaddr == NULL)
    {
        return 0;
    }

    volatile uint32_t *ioregsel =
        (volatile uint32_t *)
        ioapicaddr;

    volatile uint32_t *iowin =
        (volatile uint32_t *)(
            (uintptr_t)ioapicaddr +
            0x10
        );

    *ioregsel =
        reg & 0xFF;

    return *iowin;
}

void cpuWriteIoApic(
    void *ioapicaddr,
    uint32_t reg,
    uint32_t value
)
{
    if (ioapicaddr == NULL)
    {
        return;
    }

    volatile uint32_t *ioregsel =
        (volatile uint32_t *)
        ioapicaddr;

    volatile uint32_t *iowin =
        (volatile uint32_t *)(
            (uintptr_t)ioapicaddr +
            0x10
        );

    *ioregsel =
        reg & 0xFF;

    *iowin =
        value;
}