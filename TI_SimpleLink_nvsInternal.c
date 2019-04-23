/*
 * Copyright (c) 2017-2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Author: Mahir Hasan
 */

/*
 * NVS:
 * The flash size of CC1310F128 is 128 KB, divided into 32 sectors or pages.
 * Each sector or page size is 4 KB. It is hardware specific.
 * A 'page' refers to the smallest unit of non-volatile storage that can be
 * erased at one time, and the page size is the size of this unit.
 * During any write operation, whole page or sector is erased and written.
 *
 * TI-RTOS NVS driver:
 * Block size <= sector size
 *
 * More details:
 * http://software-dl.ti.com/dsps/dsps_public_sw/sdo_sb/targetcontent/tirtos/2_20_00_06/exports/tirtos_full_2_20_00_06/products/tidrivers_full_2_20_00_08/docs/doxygen/html/_n_v_s_8h.html
 *
 * For programming:
 * CC1310F128 base addresses of each page:
 *
 * 0x0000 | 0x1000 | 0x2000 | 0x3000 | 0x4000
 * 0x5000 | 0x6000 | 0x7000 | 0x8000 | 0x9000
 * 0xa000 | 0xb000 | 0xc000 | 0xd000 | 0xe000
 * 0xf000 | 0x10000| 0x11000| 0x12000| 0x13000
 * 0x14000| 0x15000| 0x16000| 0x17000| 0x18000
 * 0x19000| 0x1a000| 0x1b000| 0x1c000| 0x1d000
 * 0x1e000| 0x1f000
 *
 * To Save data using NVS driver:
 * ========================
 * Modify CC1310_LAUNCHXL.c line 463 to set
 * region base address and region size.
 *
 * Default NVS_REGIONS_BASE 0x1a000
 * SECTORSIZE 0x1000 (fixed, hw specific)
 * Default REGIONSIZE (SECTORSIZE * 4)
 *
 * In this example project, the following configurations are set:
 *
 * #define NVS_REGIONS_BASE 0x2000
 * #define SECTORSIZE       0x1000
 * #define REGIONSIZE       (SECTORSIZE * 24)
 *
 * With the above configuration, the following offsets can be used
 * for R/W operation:
 *
 * ========================================
 * Actual page address  | Offset from 0x2000
 * ========================================
 *        0x2000        |         0
 *        0x3000        |       0x1000
 *        0x4000        |       0x2000
 *        0x5000        |       0x3000
 *        0x6000        |       0x4000
 *        0x7000        |       0x5000
 *        0x8000        |       0x6000
 *        0x9000        |       0x7000
 *        0xa000        |       0x8000
 *        0xb000        |       0x9000
 *        0xc000        |       0xa000
 *        0xd000        |       0xb000
 *        0xe000        |       0xc000
 *        0xf000        |       0xd000
 *        0x10000       |       0xe000
 *        0x11000       |       0xf000
 *        0x12000       |       0x10000
 *        0x13000       |       0x11000
 *        0x14000       |       0x12000
 *        0x15000       |       0x13000
 *        0x16000       |       0x14000
 *        0x17000       |       0x15000
 *        0x18000       |       0x16000
 *        0x19000       |       0x17000
 *
 * It is not possible to access a sector outside of
 * the region.
 */

/*
 *  ======== TI_SimpleLink_nvsInternal.c ========
 */

/* Dependencies for NVS.h */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Standard C Libraries */
#include <string.h>
#include <stdlib.h>

/* Driver Header files */
#include <ti/display/Display.h>
#include <ti/drivers/NVS.h>

/* Example/Board Header files */
#include "Board.h"

#define FOOTER "=================================================="

/* Success return code */
#define NVS_SOK   (0)

/*
 * Buffer placed in RAM to hold bytes read from non-volatile storage.
 * buffer[3] | buffer[2] | buffer[1] | buffer[0]
 */
static uint8_t buffer[4];

// Status of saving to NVS/reading from NVS pages
static int8_t rwStatus = 0;

// 8-bit variables for testing
const uint8_t variableA = 240;
const int8_t variableB = -65;

// 16-bit variables for testing
const uint16_t variableC = 64532;
const int16_t variableD = -6453;

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    NVS_Handle nvsHandle;
    NVS_Attrs regionAttrs;
    NVS_Params nvsParams;

    Display_Handle displayHandle;

    Display_init();
    NVS_init();

    displayHandle = Display_open(Display_Type_UART, NULL);
    if (displayHandle == NULL) {
        /* Display_open() failed */
        while (1);
    }

    NVS_Params_init(&nvsParams);
    nvsHandle = NVS_open(Board_NVSINTERNAL, &nvsParams);

    if (nvsHandle == NULL) {
        Display_printf(displayHandle, 0, 0, "NVS_open() failed.");

        return (NULL);
    }

    Display_printf(displayHandle, 0, 0, "\n");

    /*
     * This will populate a NVS_Attrs structure with properties specific
     * to a NVS_Handle such as region base address, region size,
     * and sector size.
     */
    NVS_getAttrs(nvsHandle, &regionAttrs);

    /* Display the NVS region attributes */
    Display_printf(displayHandle, 0, 0, "Region Base Address: 0x%x",
            regionAttrs.regionBase);
    Display_printf(displayHandle, 0, 0, "Sector Size: 0x%x",
            regionAttrs.sectorSize);
    Display_printf(displayHandle, 0, 0, "Region Size: 0x%x\n",
            regionAttrs.regionSize);

    // Read from page 0x12000
    rwStatus = NVS_read(nvsHandle, 0x10000, (void *) buffer, sizeof(buffer));
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Reading value from page 0x12000\n");
        uint8_t variableAtemp = buffer[0];
        Display_printf(displayHandle, 0, 0, "%u\n", variableAtemp);
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot read from page 0x12000\n");
    }


    // Read from page 0x6000
    rwStatus = NVS_read(nvsHandle, 0x4000, (void *) buffer, sizeof(buffer));
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Reading value from page 0x6000\n");
        int8_t variableBtemp = (int8_t) buffer[0];
        Display_printf(displayHandle, 0, 0, "%d\n", variableBtemp);
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot read from page 0x6000\n");
    }


    // Read from page 0x16000
    rwStatus = NVS_read(nvsHandle, 0x14000, (void *) buffer, sizeof(buffer));
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Reading value from page 0x16000\n");
        uint16_t variableCtemp = buffer[0] | (buffer[1] << 8);
        Display_printf(displayHandle, 0, 0, "%u\n", variableCtemp);
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot read from page 0x16000\n");
    }


    // Read from page 0x19000
    rwStatus = NVS_read(nvsHandle, 0x17000, (void *) buffer, sizeof(buffer));
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Reading value from page 0x19000\n");
        uint16_t variableDtempU = buffer[0] | (buffer[1] << 8);
        int16_t variableDtemp = (int16_t) variableDtempU;
        Display_printf(displayHandle, 0, 0, "%d\n", variableDtemp);
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot read from page 0x19000\n");
    }


    // Write variableA at page 0x12000
    buffer[0] = variableA;
    buffer[1] = 0xff;
    buffer[2] = 0xff;
    buffer[3] = 0xff;
    rwStatus = NVS_write(nvsHandle, 0x10000, (void *) buffer, sizeof(buffer),
                           NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Successfully written at page 0x12000\n");
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot write at page 0x12000\n");
    }

    // Write variableB at page 0x6000
    buffer[0] = (uint8_t) variableB;
    buffer[1] = 0xff;
    buffer[2] = 0xff;
    buffer[3] = 0xff;
    rwStatus = NVS_write(nvsHandle, 0x4000, (void *) buffer, sizeof(buffer),
                           NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Successfully written at page 0x6000\n");
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot write at page 0x6000\n");
    }

    // Write variableC at page 0x16000
    buffer[0] = (variableC & 0x00FF);
    buffer[1] = (variableC & 0xFF00) >> 8;
    buffer[2] = 0xff;
    buffer[3] = 0xff;
    rwStatus = NVS_write(nvsHandle, 0x14000, (void *) buffer, sizeof(buffer),
                           NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Successfully written at page 0x16000\n");
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot write at page 0x16000\n");
    }

    // Write variableD at page 0x19000
    uint16_t variableDtemp = (uint16_t) variableD;
    buffer[0] = (variableDtemp & 0x00FF);
    buffer[1] = (variableDtemp & 0xFF00) >> 8;
    buffer[2] = 0xff;
    buffer[3] = 0xff;
    rwStatus = NVS_write(nvsHandle, 0x17000, (void *) buffer, sizeof(buffer),
                           NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
    if (rwStatus == NVS_SOK) {
        Display_printf(displayHandle, 0, 0, "Successfully written at page 0x19000\n");
    }
    else {
        Display_printf(displayHandle, 0, 0, "Cannot write at page 0x19000\n");
    }

    Display_printf(displayHandle, 0, 0, "Reset the device.");
    Display_printf(displayHandle, 0, 0, FOOTER);

    return (NULL);
}
