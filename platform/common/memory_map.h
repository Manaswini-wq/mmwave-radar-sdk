/**
 * @file memory_map.h
 * @brief Common memory region definitions used across platforms
 * @details Defines default stack, heap, and buffer sizes. Platform-specific
 *          linker scripts may override these with actual addresses.
 */
#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "platform_types.h"

/* -------------------------------------------------------------------------- */
/*  Stack and Heap                                                            */
/* -------------------------------------------------------------------------- */
#define MMWAVE_STACK_SIZE           (8U * 1024U)    /**< 8 KB default stack */
#define MMWAVE_HEAP_SIZE            (32U * 1024U)   /**< 32 KB default heap */
#define MMWAVE_IRQ_STACK_SIZE       (2U * 1024U)    /**< 2 KB IRQ stack */

/* -------------------------------------------------------------------------- */
/*  ADC Data Buffers                                                          */
/* -------------------------------------------------------------------------- */
#define MMWAVE_ADC_BUF_PING_SIZE    (64U * 1024U)   /**< 64 KB ping buffer */
#define MMWAVE_ADC_BUF_PONG_SIZE    (64U * 1024U)   /**< 64 KB pong buffer */

/* -------------------------------------------------------------------------- */
/*  Processing Scratch Buffers                                                */
/* -------------------------------------------------------------------------- */
#define MMWAVE_FFT_SCRATCH_SIZE     (16U * 1024U)   /**< 16 KB FFT scratch */
#define MMWAVE_CFAR_SCRATCH_SIZE    (4U * 1024U)    /**< 4 KB CFAR scratch */
#define MMWAVE_DOA_SCRATCH_SIZE     (8U * 1024U)    /**< 8 KB DOA scratch */

/* -------------------------------------------------------------------------- */
/*  DMA Descriptor Region                                                     */
/* -------------------------------------------------------------------------- */
#define MMWAVE_DMA_DESC_SIZE        (1U * 1024U)    /**< 1 KB DMA descriptors */
#define MMWAVE_DMA_DESC_ALIGN       64U             /**< 64-byte alignment */

/* -------------------------------------------------------------------------- */
/*  Log and Debug Buffers                                                     */
/* -------------------------------------------------------------------------- */
#define MMWAVE_LOG_BUF_SIZE         (2U * 1024U)    /**< 2 KB log buffer */
#define MMWAVE_ASSERT_BUF_SIZE      256U            /**< 256 B assert info */

/* -------------------------------------------------------------------------- */
/*  Shared Memory (between R5F cores or R5F <-> DSP)                          */
/* -------------------------------------------------------------------------- */
#define MMWAVE_SHARED_MEM_SIZE      (16U * 1024U)   /**< 16 KB shared region */
#define MMWAVE_MAILBOX_SIZE         256U            /**< 256 B mailbox */

/* -------------------------------------------------------------------------- */
/*  Total Buffer Budget Sanity Check                                          */
/* -------------------------------------------------------------------------- */
#define MMWAVE_TOTAL_BUFFER_BUDGET  (MMWAVE_ADC_BUF_PING_SIZE + \
                                     MMWAVE_ADC_BUF_PONG_SIZE + \
                                     MMWAVE_FFT_SCRATCH_SIZE  + \
                                     MMWAVE_CFAR_SCRATCH_SIZE + \
                                     MMWAVE_DOA_SCRATCH_SIZE  + \
                                     MMWAVE_DMA_DESC_SIZE     + \
                                     MMWAVE_SHARED_MEM_SIZE)

#endif /* MEMORY_MAP_H */
