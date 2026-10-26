/**
 * @file compiler_abstraction.h
 * @brief Compiler abstraction macros for TI CGT and GCC
 * @details Provides portable macros for inline, restrict, packed structs,
 *          section placement, weak linkage, and alignment directives.
 */
#ifndef COMPILER_ABSTRACTION_H
#define COMPILER_ABSTRACTION_H

/* -------------------------------------------------------------------------- */
/*  Compiler Detection                                                        */
/* -------------------------------------------------------------------------- */

#if defined(__TI_COMPILER_VERSION__)
    /* TI ARM CGT (Code Generation Tools) */
    #define MMWAVE_INLINE       static inline
    #define MMWAVE_RESTRICT     restrict
    #define MMWAVE_PACKED       __attribute__((packed))
    #define MMWAVE_SECTION(s)   __attribute__((section(s)))
    #define MMWAVE_WEAK         __attribute__((weak))
    #define MMWAVE_ALIGN(n)     __attribute__((aligned(n)))
    #define MMWAVE_NORETURN     __attribute__((noreturn))
    #define MMWAVE_UNUSED       __attribute__((unused))
#elif defined(__GNUC__)
    /* GCC / arm-none-eabi-gcc */
    #define MMWAVE_INLINE       static inline __attribute__((always_inline))
    #define MMWAVE_RESTRICT     __restrict__
    #define MMWAVE_PACKED       __attribute__((packed))
    #define MMWAVE_SECTION(s)   __attribute__((section(s)))
    #define MMWAVE_WEAK         __attribute__((weak))
    #define MMWAVE_ALIGN(n)     __attribute__((aligned(n)))
    #define MMWAVE_NORETURN     __attribute__((noreturn))
    #define MMWAVE_UNUSED       __attribute__((unused))
#else
    /* Fallback for unknown compilers */
    #warning "Unknown compiler - abstraction macros may not work correctly"
    #define MMWAVE_INLINE       static inline
    #define MMWAVE_RESTRICT
    #define MMWAVE_PACKED
    #define MMWAVE_SECTION(s)
    #define MMWAVE_WEAK
    #define MMWAVE_ALIGN(n)
    #define MMWAVE_NORETURN
    #define MMWAVE_UNUSED
#endif

#endif /* COMPILER_ABSTRACTION_H */
