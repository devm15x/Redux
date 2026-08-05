#include "ata.h"
#include "io.h"

#include <stdint.h>
#include <stdbool.h>

#define ATA_PRIMARY_IO       0x1F0
#define ATA_PRIMARY_CONTROL  0x3F6

#define ATA_REG_DATA         0x00
#define ATA_REG_ERROR        0x01
#define ATA_REG_SECTOR_COUNT 0x02
#define ATA_REG_LBA_LOW      0x03
#define ATA_REG_LBA_MID      0x04
#define ATA_REG_LBA_HIGH     0x05
#define ATA_REG_DRIVE        0x06
#define ATA_REG_STATUS       0x07
#define ATA_REG_COMMAND      0x07

#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_READ_PIO     0x20

#define ATA_STATUS_ERROR     0x01
#define ATA_STATUS_DRQ       0x08
#define ATA_STATUS_DF        0x20
#define ATA_STATUS_BUSY      0x80

#define ATA_CMD_WRITE_PIO    0x30
#define ATA_CMD_CACHE_FLUSH  0xE7

static uint16_t inw(uint16_t port)
{
    uint16_t value;

    __asm__ volatile(
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void ata_delay_400ns(void)
{
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
}

static bool ata_wait_not_busy(void)
{
    for (uint32_t timeout = 0; timeout < 1000000; timeout++)
    {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);

        if ((status & ATA_STATUS_BUSY) == 0)
        {
            return true;
        }
    }

    return false;
}

static bool ata_wait_for_data(void)
{
    for (uint32_t timeout = 0; timeout < 1000000; timeout++)
    {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);

        if (status & ATA_STATUS_ERROR)
        {
            return false;
        }

        if (status & ATA_STATUS_DF)
        {
            return false;
        }

        if ((status & ATA_STATUS_BUSY) == 0 &&
            (status & ATA_STATUS_DRQ) != 0)
        {
            return true;
        }
    }

    return false;
}

bool ata_initialize(void)
{
    outb(
        ATA_PRIMARY_IO + ATA_REG_DRIVE,
        0xA0
    );

    ata_delay_400ns();

    outb(ATA_PRIMARY_IO + ATA_REG_SECTOR_COUNT, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LOW, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HIGH, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(
        ATA_PRIMARY_IO + ATA_REG_STATUS
    );

    if (status == 0)
    {
        return false;
    }

    if (!ata_wait_not_busy())
    {
        return false;
    }

    uint8_t lba_mid = inb(
        ATA_PRIMARY_IO + ATA_REG_LBA_MID
    );

    uint8_t lba_high = inb(
        ATA_PRIMARY_IO + ATA_REG_LBA_HIGH
    );

    if (lba_mid != 0 || lba_high != 0)
    {
        return false;
    }

    if (!ata_wait_for_data())
    {
        return false;
    }

    for (uint16_t i = 0; i < 256; i++)
    {
        (void)inw(
            ATA_PRIMARY_IO + ATA_REG_DATA
        );
    }

    return true;
}

bool ata_read_sectors(
    uint32_t lba,
    uint8_t sector_count,
    void *buffer
)
{
    if (sector_count == 0)
    {
        return false;
    }

    if (!ata_wait_not_busy())
    {
        return false;
    }

    outb(
        ATA_PRIMARY_IO + ATA_REG_DRIVE,
        (uint8_t)(0xE0 | ((lba >> 24) & 0x0F))
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_SECTOR_COUNT,
        sector_count
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA_LOW,
        (uint8_t)(lba & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA_MID,
        (uint8_t)((lba >> 8) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA_HIGH,
        (uint8_t)((lba >> 16) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_COMMAND,
        ATA_CMD_READ_PIO
    );

    uint16_t *destination = buffer;

    for (uint8_t sector = 0;
         sector < sector_count;
         sector++)
    {
        if (!ata_wait_for_data())
        {
            return false;
        }

        for (uint16_t word = 0; word < 256; word++)
        {
            *destination++ = inw(
                ATA_PRIMARY_IO + ATA_REG_DATA
            );
        }

        ata_delay_400ns();
    }

    return true;
}
static bool ata_flush_cache(void)
{
    outb(
        ATA_PRIMARY_IO + ATA_REG_COMMAND,
        ATA_CMD_CACHE_FLUSH
    );

    return ata_wait_not_busy();
}

bool ata_write_sectors(
    uint32_t lba,
    uint8_t sector_count,
    const void *buffer
)
{
    if (sector_count == 0 || buffer == 0)
    {
        return false;
    }

    if (lba > 0x0FFFFFFF)
    {
        return false;
    }

    if (!ata_wait_not_busy())
    {
        return false;
    }

    outb(
        ATA_PRIMARY_IO + ATA_REG_DRIVE,
        (uint8_t)(0xE0 | ((lba >> 24) & 0x0F))
    );

    ata_delay_400ns();

    outb(
        ATA_PRIMARY_IO + ATA_REG_SECTOR_COUNT,
        sector_count
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA_LOW,
        (uint8_t)(lba & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA_MID,
        (uint8_t)((lba >> 8) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA_HIGH,
        (uint8_t)((lba >> 16) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_COMMAND,
        ATA_CMD_WRITE_PIO
    );

    const uint16_t *source = buffer;

    for (uint8_t sector = 0;
         sector < sector_count;
         sector++)
    {
        if (!ata_wait_for_data())
        {
            return false;
        }

        for (uint16_t word = 0;
             word < 256;
             word++)
        {
            outw(
                ATA_PRIMARY_IO + ATA_REG_DATA,
                *source++
            );
        }

        ata_delay_400ns();
    }

    return ata_flush_cache();
}