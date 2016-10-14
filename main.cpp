/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"

// ----------------------------------------------------------------
// GENERAL COMPILE-TIME CONSTANTS
// ----------------------------------------------------------------

// Things to do with the processing system
#define SYSTEM_CONTROL_BLOCK_START_ADDRESS ((uint32_t *) 0xe000ed00)
#define SYSTEM_RAM_SIZE_BYTES 20480

// ----------------------------------------------------------------
// TYPES
// ----------------------------------------------------------------

// Tick callback
typedef void (*TickCallback_t)(uint32_t count);

// ----------------------------------------------------------------
// GLOBAL VARIABLES
// ----------------------------------------------------------------

// GPIO to toggle
static DigitalOut gGpio(LED1);

// Flipper to test uS delays
static Ticker gFlipper;

// Serial port for talking to a PC
static RawSerial gUsb (USBTX, USBRX);

// ----------------------------------------------------------------
// FUNCTION PROTOTYPES
// ----------------------------------------------------------------

static void checkCpu(void);
static void * mallocLargestSize(size_t *pSizeBytes);
static size_t checkHeapSize(size_t sizeBytes);
static void checkRam(uint32_t * pMem, size_t memorySizeBytes);
static void flip(void);

// ----------------------------------------------------------------
// STATIC FUNCTIONS
// ----------------------------------------------------------------

// Check-out the characteristics of the CPU we're running on
static void checkCpu()
{
    uint32_t x = 0x01234567;

    printf("\n*** Printing stuff of interest about the CPU.\n");
    if ((*(uint8_t *) &x) == 0x67)
    {
        printf("Little endian.\n");
    }
    else
    {
        printf("Big endian.\n");
    }

    // Read the system control block
    // CPU ID register
    printf("CPUID: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS));
    // Interrupt control and state register
    printf("ICSR: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS + 1));
    // VTOR is not there, skip it
    // Application interrupt and reset control register
    printf("AIRCR: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS + 3));
    // SCR is not there, skip it
    // Configuration and control register
    printf("CCR: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS + 5));
    // System handler priority register 2
    printf("SHPR2: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS + 6));
    // System handler priority register 3
    printf("SHPR3: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS + 7));
    // System handler control and status register
    printf("SHCSR: 0x%08lx.\n", *(SYSTEM_CONTROL_BLOCK_START_ADDRESS + 8));

    printf("Last stack entry was at 0x%08lx.\n", (uint32_t) &x);
    printf("A static variable is at 0x%08lx.\n", (uint32_t) &gFlipper);
}

// Malloc the largest block possible.  When called pSizeBytes should
// point to the target size required and on return pSizeBytes will be filled
// in with the actual size allocated.
// A pointer to the mallocated block is returned.
static void * mallocLargestSize(size_t *pSizeBytes)
{
    int32_t memorySizeBytes;
    void * pMem = NULL;

    if (pSizeBytes != NULL)
    {
        memorySizeBytes = (int32_t) *pSizeBytes;

        while ((pMem == NULL) && (memorySizeBytes > 0))
        {
            pMem = malloc(memorySizeBytes);
            if (pMem == NULL)
            {
                memorySizeBytes -= sizeof(uint32_t);
            }
        }

        if (memorySizeBytes < 0)
        {
            memorySizeBytes = 0;
        }

        *pSizeBytes = memorySizeBytes;
    }

    return pMem;
}

// Check how much heap can be malloc'ed, up to sizeBytes in size.
// Returns the number of bytes successfully malloc'ed.
static size_t checkHeapSize(size_t sizeBytes)
{
    size_t totalHeapSizeBytes = 0;
    void *pFirstMalloc = NULL;
    size_t firstMallocSizeBytes = sizeBytes;
    void **ppLaterMalloc = NULL;
    size_t laterMallocSizeBytes = sizeBytes;

    // Try to allocate a block
    pFirstMalloc = mallocLargestSize(&firstMallocSizeBytes);

    if (pFirstMalloc != NULL)
    {
        // Check that this bit of RAM is good
        checkRam((uint32_t *) pFirstMalloc, firstMallocSizeBytes);

        // Now use the block to store pointers to memory and
        // try to allocate more blocks.  This is necessary
        // since malloc() may be limited in what it can grab
        // in one go.
        totalHeapSizeBytes += firstMallocSizeBytes;

        ppLaterMalloc = (void **) pFirstMalloc;
        laterMallocSizeBytes = SYSTEM_RAM_SIZE_BYTES;

        while ((ppLaterMalloc < (void **) pFirstMalloc + (firstMallocSizeBytes / sizeof (void **))) && (*ppLaterMalloc != NULL) && (laterMallocSizeBytes > 0))
        {
            *ppLaterMalloc = mallocLargestSize(&laterMallocSizeBytes);

            if (*ppLaterMalloc != NULL)
            {
                // Check that this bit of RAM is good
                checkRam((uint32_t *) *ppLaterMalloc, laterMallocSizeBytes);

                totalHeapSizeBytes += laterMallocSizeBytes;
                laterMallocSizeBytes = SYSTEM_RAM_SIZE_BYTES;
                ppLaterMalloc++;
            }
        }

        // Free up the mallocated memory
        while (ppLaterMalloc >= pFirstMalloc)
        {
            free(*ppLaterMalloc);
            ppLaterMalloc--;
        }

        free(pFirstMalloc);
    }

    return totalHeapSizeBytes;
}

// Check that the given area of RAM is good.  Prints an error
// message and stops dead if there is a problem.
static void checkRam(uint32_t *pMem, size_t memorySizeBytes)
{
    uint32_t * pLocation = NULL;
    uint32_t value;

    if (pMem != NULL)
    {
        printf("*** Checking RAM, from 0x%08lx to 0x%08lx.\n", (uint32_t) pMem, (uint32_t) pMem + memorySizeBytes / sizeof (*pMem));

        // Write a walking 1 pattern
        value = 1;
        for (pLocation = pMem; pLocation < pMem + memorySizeBytes / sizeof (*pLocation); pLocation++)
        {
            *pLocation = value;
            value <<= 1;
            if (value == 0)
            {
                value = 1;
            }
        }

        // Read the walking 1 pattern
        value = 1;
        for (pLocation = pMem; pLocation < pMem + memorySizeBytes / sizeof (*pLocation); pLocation++)
        {
            value <<= 1;
            if (value == 0)
            {
                value = 1;
            }
        }

        if (pLocation >= pMem + memorySizeBytes / sizeof (uint32_t))
        {
            // Write an inverted walking 1 pattern
            value = 1;
            for (pLocation = pMem; pLocation < pMem + memorySizeBytes / sizeof (*pLocation); pLocation++)
            {
                *pLocation = ~value;
                value <<= 1;
                if (value == 0)
                {
                    value = 1;
                }
            }

            // Read the inverted walking 1 pattern
            value = 1;
            for (pLocation = pMem; (pLocation < pMem + memorySizeBytes / sizeof (*pLocation)) && (*pLocation == ~value); pLocation++)
            {
                value <<= 1;
                if (value == 0)
                {
                    value = 1;
                }
            }
        }

        if (pLocation < pMem + memorySizeBytes / sizeof (*pLocation))
        {
            printf("!!! RAM check failure at location 0x%08lx (contents 0x%08lx).\n", (uint32_t) pLocation, *pLocation);
        }
    }
}

// Flip
static void flip()
{
    gGpio = !gGpio;
}

// ----------------------------------------------------------------
// PUBLIC FUNCTIONS
// ----------------------------------------------------------------

int main(void)
{
    size_t memorySizeBytes;

    //gUsb.baud (115200);
    gUsb.baud (9600);

    checkCpu();

    printf("*** Checking heap size available.\n");
    memorySizeBytes = checkHeapSize(SYSTEM_RAM_SIZE_BYTES);

    printf("*** Total heap available was %d bytes.\n", memorySizeBytes);
    printf("    The last variable pushed onto the stack was at 0x%08lx, MSP is at 0x%08lx.\n", (uint32_t) &memorySizeBytes, __get_MSP());

    printf("*** Running us_ticker at 100 usecond intervals for 2 seconds...\n");

    /* Use a usecond delay function to check-out the us_ticker at high speed for a little while */
    gFlipper.attach_us(&flip, 100);

    wait(2);

    gFlipper.detach();

    printf("*** Echoing received characters forever.\n");

    while (1)
    {
        if (gUsb.readable() && gUsb.writeable())
        {
            char c = gUsb.getc();
            gUsb.putc(c);
        }
    }
}
