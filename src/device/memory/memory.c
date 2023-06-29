/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.c                                                *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "memory.h"

#include "api/callbacks.h"
#include "api/m64p_types.h"

#include "device/device.h"
#include "device/rcp/rsp/rsp_core.h"
#include "device/pif/pif.h"

#ifdef DBG
#include <string.h>

#include "device/r4300/r4300_core.h"

#include "debugger/dbg_breakpoints.h"
#include "debugger/dbg_memory.h"
#endif

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <malloc.h>
#endif

#ifdef DBG
enum
{
    BP_CHECK_READ  = 0x1,
    BP_CHECK_WRITE = 0x2,
};

void read_with_bp_checks(void* opaque, uint32_t address, uint32_t* value)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;
    uint16_t region = address >> 16;

    /* only check bp if active */
    if (r4300->mem->bp_checks[region] & BP_CHECK_READ) {
        check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
                M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);
    }

    mem_read32(&r4300->mem->saved_handlers[region], address, value);
}

void write_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;
    uint16_t region = address >> 16;

    /* only check bp if active */
    if (r4300->mem->bp_checks[region] & BP_CHECK_WRITE) {
        check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
                M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);
    }

    mem_write32(&r4300->mem->saved_handlers[region], address, value, mask);
}

void activate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* dbg_handler = &mem->dbg_handler;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* if neither read nor write bp is active, set dbg_handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *saved_handler = *handler;
        *handler = *dbg_handler;
    }

    /* activate bp read */
    *bp_check |= BP_CHECK_READ;
}

void deactivate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* desactivate bp read */
    *bp_check &= ~BP_CHECK_READ;

    /* if neither read nor write bp is active, restore handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *handler = *saved_handler;
    }
}

void activate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* dbg_handler = &mem->dbg_handler;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* if neither read nor write bp is active, set dbg_handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *saved_handler = *handler;
        *handler = *dbg_handler;
    }

    /* activate bp write */
    *bp_check |= BP_CHECK_WRITE;
}

void deactivate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* desactivate bp write */
    *bp_check &= ~BP_CHECK_WRITE;

    /* if neither read nor write bp is active, restore handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *handler = *saved_handler;
    }
}

int get_memory_type(struct memory* mem, uint32_t address)
{
    return mem->memtype[address >> 16];
}
#else
void read_with_bp_checks(void* opaque, uint32_t address, uint32_t* value)
{
}

void write_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
}
#endif

void init_memory(struct memory* mem,
                 struct mem_mapping* mappings, size_t mappings_count,
                 void* base,
                 struct mem_handler* dbg_handler)
{
    size_t m;

#ifdef DBG
    memset(mem->bp_checks, 0, 0x10000*sizeof(mem->bp_checks[0]));
    memcpy(&mem->dbg_handler, dbg_handler, sizeof(*dbg_handler));
#endif

    mem->base = base;

    for(m = 0; m < mappings_count; ++m) {
        apply_mem_mapping(mem, &mappings[m]);
    }
}

static void map_region(struct memory* mem,
                       uint16_t region,
                       int type,
                       const struct mem_handler* handler)
{
#ifdef DBG
    /* set region type */
    mem->memtype[region] = type;

    /* set handler */
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED) != -1)
    {
        mem->saved_handlers[region] = *handler;
        mem->handlers[region] = mem->dbg_handler;
    }
    else
#endif
    {
        (void)type;
        mem->handlers[region] = *handler;
    }
}

void apply_mem_mapping(struct memory* mem, const struct mem_mapping* mapping)
{
    size_t i;
    uint16_t begin = mapping->begin >> 16;
    uint16_t end   = mapping->end   >> 16;

    for (i = begin; i <= end; ++i) {
        map_region(mem, i, mapping->type, &mapping->handler);
    }
}

int init_mem_base(MemoryBase* mem_base) {
#ifdef _WIN32
    mem_base->rdram = _aligned_malloc(RDRAM_MEMORY_SIZE, MB_RDRAM_DRAM_ALIGNMENT_REQUIREMENT);
    if (mem_base->rdram == NULL) {
        DebugMessage(M64MSG_ERROR, "Failed to allocate rdram");
        return 1;
    }

    mem_base->cartrom = _aligned_malloc(CART_ROM_MAX_SIZE, MB_ALIGNMENT_REQUIREMENT);
    if (mem_base->cartrom == NULL) {
        DebugMessage(M64MSG_ERROR, "Failed to allocate cartrom");
        return 1;
    }

    mem_base->rspmem = _aligned_malloc(SP_MEM_SIZE, MB_ALIGNMENT_REQUIREMENT);
    if (mem_base->rspmem == NULL) {
        DebugMessage(M64MSG_ERROR, "Failed to allocate rspmem");
        return 1;
    }

    mem_base->ddrom = _aligned_malloc(DD_ROM_MAX_SIZE, MB_ALIGNMENT_REQUIREMENT);
    if (mem_base->ddrom == NULL) {
        DebugMessage(M64MSG_ERROR, "Failed to allocate ddrom");
        return 1;
    }

    mem_base->pifmem = _aligned_malloc(PIF_ROM_SIZE + PIF_RAM_SIZE, MB_ALIGNMENT_REQUIREMENT);
    if (mem_base->pifmem == NULL) {
        DebugMessage(M64MSG_ERROR, "Failed to allocate pifmem");
        return 1;
    }
#else
    if (posix_memalign(&mem_base->rdram, MB_RDRAM_DRAM_ALIGNMENT_REQUIREMENT, RDRAM_MEMORY_SIZE) != 0) {
        mem_base->rdram = NULL;
        DebugMessage(M64MSG_ERROR, "Failed to allocate rdram");
        return 1;
    }

    if (posix_memalign(&mem_base->cartrom, MB_ALIGNMENT_REQUIREMENT, CART_ROM_MAX_SIZE) != 0) {
        mem_base->cartrom = NULL;
        DebugMessage(M64MSG_ERROR, "Failed to allocate cartrom");
        return 1;
    }

    if (posix_memalign(&mem_base->rspmem, MB_ALIGNMENT_REQUIREMENT, SP_MEM_SIZE) != 0) {
        mem_base->rspmem = NULL;
        DebugMessage(M64MSG_ERROR, "Failed to allocate rspmem");
        return 1;
    }

    if (posix_memalign(&mem_base->ddrom, MB_ALIGNMENT_REQUIREMENT, DD_ROM_MAX_SIZE) != 0) {
        mem_base->ddrom = NULL;
        DebugMessage(M64MSG_ERROR, "Failed to allocate ddrom");
        return 1;
    }

    if (posix_memalign(&mem_base->pifmem, MB_ALIGNMENT_REQUIREMENT, PIF_ROM_SIZE + PIF_RAM_SIZE) != 0) {
        mem_base->pifmem = NULL;
        DebugMessage(M64MSG_ERROR, "Failed to allocate pifmem");
        return 1;
    }
#endif

    return 0;
}

void release_mem_base(MemoryBase* mem_base) {
#ifdef _WIN32
    _aligned_free(mem_base->rdram);
    _aligned_free(mem_base->cartrom);
    _aligned_free(mem_base->rspmem);
    _aligned_free(mem_base->ddrom);
    _aligned_free(mem_base->pifmem);
#else
    free(mem_base->rdram);
    free(mem_base->cartrom);
    free(mem_base->rspmem);
    free(mem_base->ddrom);
    free(mem_base->pifmem);
#endif
}

uint32_t* mem_base_u32(MemoryBase* mem_base, uint32_t address) {
    uint32_t* mem = NULL;

    if (address < MM_RDRAM_REGS) {
        mem = (uint32_t*)(((uint8_t*)mem_base->rdram) + (address - MM_RDRAM_DRAM));
    }
    else if (address >= MM_CART_ROM) {
        if ((address & UINT32_C(0xfff00000)) == MM_PIF_MEM) {
            mem = (uint32_t*)((uint8_t*)mem_base->pifmem + (address - MM_PIF_MEM));
        }
        else {
            mem = (uint32_t*)((uint8_t*)mem_base->cartrom + (address - MM_CART_ROM));
        }
    }
    else if ((address & UINT32_C(0xfe000000)) ==  MM_DD_ROM) {
        mem = (uint32_t*)((uint8_t*)mem_base->ddrom + (address - MM_DD_ROM));
    }
    else if ((address & UINT32_C(0xffffe000)) == MM_RSP_MEM) {
        mem = (uint32_t*)((uint8_t*)mem_base->rspmem + (address - MM_RSP_MEM));
    }
    else if (address >= MM_RDRAM_DRAM2) {
        mem = (uint32_t*)((uint8_t*)mem_base->rdram + (address - MM_RDRAM_DRAM2 + MM_RDRAM_REGS));
    }
    else {
        mem = NULL;
    }

    return mem;
}
