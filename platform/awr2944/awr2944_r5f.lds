/**
 * @file awr2944_r5f.lds
 * @brief Linker Script for AWR2944 R5F Core
 * @details Memory layout for Cortex-R5F on AWR2944:
 *          TCMA (32 KB), TCMB (32 KB), L2RAM (256 KB),
 *          L3RAM (512 KB), FLASH (2 MB)
 */

MEMORY
{
    VECTORS (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00000100  /*   256 B  Vectors in TCMA  */
    TCMA    (rwx) : ORIGIN = 0x00000100, LENGTH = 0x00007F00  /*  ~32 KB  Tightly Coupled A */
    TCMB    (rw)  : ORIGIN = 0x00080000, LENGTH = 0x00008000  /*  32 KB  Tightly Coupled B */
    L2RAM   (rwx) : ORIGIN = 0x50800000, LENGTH = 0x00040000  /* 256 KB  L2 SRAM           */
    L3RAM   (rw)  : ORIGIN = 0x51000000, LENGTH = 0x00080000  /* 512 KB  L3 SRAM           */
    FLASH   (rx)  : ORIGIN = 0x60000000, LENGTH = 0x00200000  /*   2 MB  External Flash    */
}

SECTIONS
{
    /* Interrupt vector table - must be at address 0 */
    .vectors : ALIGN(256)
    {
        *(.vectors)
        *(.vectors.*)
    } > VECTORS

    /* Executable code */
    .text : ALIGN(4)
    {
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
    } > L2RAM

    /* Read-only data */
    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
    } > L2RAM

    /* Initialized data */
    .data : ALIGN(4)
    {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } > L2RAM

    /* Zero-initialized data */
    .bss (NOLOAD) : ALIGN(4)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } > L2RAM

    /* Stack in TCMB for fast access */
    .stack (NOLOAD) : ALIGN(8)
    {
        __stack_start = .;
        . = . + 0x2000;  /* 8 KB stack */
        __stack_end = .;
    } > TCMB

    /* Heap in L3RAM for large allocations */
    .heap (NOLOAD) : ALIGN(8)
    {
        __heap_start = .;
        . = . + 0x8000;  /* 32 KB heap */
        __heap_end = .;
    } > L3RAM

    /* ADC ping/pong buffers in L3RAM */
    .adc_buffers (NOLOAD) : ALIGN(64)
    {
        *(.adc_ping)
        *(.adc_pong)
    } > L3RAM

    /* DMA descriptors in TCMA for fast access */
    .dma_desc (NOLOAD) : ALIGN(64)
    {
        *(.dma_desc)
    } > TCMA

    /* Discard debug sections in release builds */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

/* Entry point */
ENTRY(_reset_handler)

/* Export stack pointer for startup code */
__stack_top = __stack_end;
