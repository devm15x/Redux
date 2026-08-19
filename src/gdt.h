#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20

#define GDT_USER_CODE_R3 (GDT_USER_CODE | 3)
#define GDT_USER_DATA_R3 (GDT_USER_DATA | 3)

void gdt_init(void);