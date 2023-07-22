#include "api/memoryexport.h"
#include "device/device.h"

extern struct device g_dev;

extern u32* mem_base_u32(MemoryBase* mem_base, uint32_t address);

EXPORT void* CALL Memory_GetBaseAddress(void) {
    return g_mem_base.rdram;
}

EXPORT void* CALL ROM_GetBaseAddress(void) {
    return g_dev.cart.cart_rom.rom;
}

EXPORT u32 CALL ROM_GetBaseSize(void) {
    return g_dev.cart.cart_rom.rom_size;
}

EXPORT void CALL InvalidateCachedCode(void) {
#ifdef NEW_DYNAREC
    invalidate_cached_code_new_dynarec(&g_dev.r4300, 0, 0);
#else
    invalidate_r4300_cached_code(&g_dev.r4300, 0, 0);
#endif
}
