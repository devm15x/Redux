#include "ff.h"
#include "diskio.h"
#include "ata.h"

#include <stdint.h>

#define DEV_ATA 0
#define ATA_SECTOR_SIZE 512
#define ATA_DISK_SECTOR_COUNT 262144

static DSTATUS ata_status = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != DEV_ATA)
    {
        return STA_NOINIT;
    }

    return ata_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != DEV_ATA)
    {
        return STA_NOINIT;
    }

    if (!ata_initialize())
    {
        ata_status = STA_NOINIT;
        return ata_status;
    }

    ata_status = 0;
    return ata_status;
}

DRESULT disk_read(
    BYTE pdrv,
    BYTE *buff,
    LBA_t sector,
    UINT count
)
{
    if (pdrv != DEV_ATA)
    {
        return RES_PARERR;
    }

    if (buff == 0 || count == 0)
    {
        return RES_PARERR;
    }

    if (ata_status & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    while (count > 0)
    {
        UINT chunk = count;

        if (chunk > 255)
        {
            chunk = 255;
        }

        if (!ata_read_sectors(
                (uint32_t)sector,
                (uint8_t)chunk,
                buff))
        {
            return RES_ERROR;
        }

        sector += chunk;
        buff += chunk * ATA_SECTOR_SIZE;
        count -= chunk;
    }

    return RES_OK;
}

#if FF_FS_READONLY == 0

DRESULT disk_write(
    BYTE pdrv,
    const BYTE *buff,
    LBA_t sector,
    UINT count
)
{
    if (pdrv != DEV_ATA)
    {
        return RES_PARERR;
    }

    if (buff == 0 || count == 0)
    {
        return RES_PARERR;
    }

    if (ata_status & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    while (count > 0)
    {
        UINT chunk = count;

        if (chunk > 255)
        {
            chunk = 255;
        }

        if (!ata_write_sectors(
                (uint32_t)sector,
                (uint8_t)chunk,
                buff))
        {
            return RES_ERROR;
        }

        sector += chunk;
        buff += chunk * ATA_SECTOR_SIZE;
        count -= chunk;
    }

    return RES_OK;
}

#endif

DRESULT disk_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff
)
{
    if (pdrv != DEV_ATA)
    {
        return RES_PARERR;
    }

    if (ata_status & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            if (buff == 0)
            {
                return RES_PARERR;
            }

            *(LBA_t *)buff = ATA_DISK_SECTOR_COUNT;
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (buff == 0)
            {
                return RES_PARERR;
            }

            *(WORD *)buff = ATA_SECTOR_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (buff == 0)
            {
                return RES_PARERR;
            }

            *(DWORD *)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    return ((DWORD)(2026 - 1980) << 25)
         | ((DWORD)8 << 21)
         | ((DWORD)5 << 16)
         | ((DWORD)12 << 11)
         | ((DWORD)0 << 5)
         | ((DWORD)0 >> 1);
}