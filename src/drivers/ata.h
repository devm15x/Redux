#ifndef REDUX_ATA_H
#define REDUX_ATA_H

#include <stdint.h>
#include <stdbool.h>

bool ata_initialize(void);

bool ata_read_sectors(
    uint32_t lba,
    uint8_t sector_count,
    void *buffer
);

bool ata_write_sectors(
    uint32_t lba,
    uint8_t sector_count,
    const void *buffer
);


#endif