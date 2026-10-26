;; @file fft_window.S
;; @brief Assembly-optimized FFT windowing for ARM Cortex-R4/R5 DSP
;; @details Uses ARM DSP SIMD instructions (SMULBB, SMULTT, SSAT, PKHBT)
;;          for parallel multiply of complex I/Q radar ADC samples by
;;          pre-computed window coefficients. Processes 4 complex samples
;;          per loop iteration using packed halfword operations.
;;
;; Calling convention: AAPCS (ARM Architecture Procedure Call Standard)
;; void fft_apply_window(int16_t* data, const int16_t* window, uint32_t num_samples)
;;   R0 = data    – interleaved I/Q: [I0,Q0,I1,Q1,...] (in-place)
;;   R1 = window  – real window coefficients in Q15: [w0,w0,w1,w1,...] (duplicated)
;;   R2 = num_samples (number of complex samples, must be multiple of 4)

    .syntax unified
    .arch   armv7-r
    .fpu    vfpv3-d16
    .global fft_apply_window
    .text
    .align  5                           @ 32-byte alignment for cache-line fetch

fft_apply_window:
    PUSH    {R4-R11, LR}               @ Save callee-saved registers

    @ ---- Argument validation ----
    CMP     R2, #4                      @ At least 4 samples required
    BLT     .Lexit                      @ Bail out if fewer than 4

    @ Ensure num_samples is multiple of 4 by masking lower 2 bits
    BIC     R2, R2, #3                  @ R2 = num_samples & ~3

    @ R3 = loop counter (number of 4-sample groups)
    LSR     R3, R2, #2                  @ R3 = num_samples / 4

    @ Prefetch first cache lines of data and window
    PLD     [R0, #0]
    PLD     [R0, #32]
    PLD     [R1, #0]
    PLD     [R1, #32]

.Lloop4:
    @ ---- Load 4 complex samples (8 halfwords = 16 bytes) ----
    @ Each complex sample is packed as [I, Q] in one 32-bit word
    LDR     R4, [R0, #0]               @ R4 = {Q0, I0}  (packed halfwords)
    LDR     R5, [R0, #4]               @ R5 = {Q1, I1}
    LDR     R6, [R0, #8]               @ R6 = {Q2, I2}
    LDR     R7, [R0, #12]              @ R7 = {Q3, I3}

    @ ---- Load 4 window coefficients (duplicated: [wk, wk]) ----
    LDR     R8,  [R1, #0]              @ R8  = {w0, w0}
    LDR     R9,  [R1, #4]              @ R9  = {w1, w1}
    LDR     R10, [R1, #8]              @ R10 = {w2, w2}
    LDR     R11, [R1, #12]             @ R11 = {w3, w3}

    @ ---- Sample 0: multiply I0*w0 and Q0*w0 ----
    SMULBB  LR, R4, R8                 @ LR = I0 * w0 (bottom * bottom)
    ASR     LR, LR, #15                @ Shift Q15 product back to Q0
    SSAT    LR, #16, LR                @ Saturate to 16-bit signed
    MOV     R4, R4, LSR #16            @ Isolate Q0 in low halfword
    SMULBB  R4, R4, R8                 @ R4 = Q0 * w0
    ASR     R4, R4, #15
    SSAT    R4, #16, R4
    PKHBT   R4, LR, R4, LSL #16        @ R4 = {Q0*w0, I0*w0} re-packed

    @ ---- Sample 1: multiply I1*w1 and Q1*w1 ----
    SMULBB  LR, R5, R9                 @ LR = I1 * w1
    ASR     LR, LR, #15
    SSAT    LR, #16, LR
    MOV     R5, R5, LSR #16
    SMULBB  R5, R5, R9                 @ R5 = Q1 * w1
    ASR     R5, R5, #15
    SSAT    R5, #16, R5
    PKHBT   R5, LR, R5, LSL #16        @ Re-pack sample 1

    @ ---- Sample 2: multiply I2*w2 and Q2*w2 ----
    SMULBB  LR, R6, R10                @ LR = I2 * w2
    ASR     LR, LR, #15
    SSAT    LR, #16, LR
    MOV     R6, R6, LSR #16
    SMULBB  R6, R6, R10                @ R6 = Q2 * w2
    ASR     R6, R6, #15
    SSAT    R6, #16, R6
    PKHBT   R6, LR, R6, LSL #16        @ Re-pack sample 2

    @ ---- Sample 3: multiply I3*w3 and Q3*w3 ----
    SMULBB  LR, R7, R11                @ LR = I3 * w3
    ASR     LR, LR, #15
    SSAT    LR, #16, LR
    MOV     R7, R7, LSR #16
    SMULBB  R7, R7, R11                @ R7 = Q3 * w3
    ASR     R7, R7, #15
    SSAT    R7, #16, R7
    PKHBT   R7, LR, R7, LSL #16        @ Re-pack sample 3

    @ ---- Store 4 windowed complex samples back ----
    STR     R4, [R0, #0]
    STR     R5, [R0, #4]
    STR     R6, [R0, #8]
    STR     R7, [R0, #12]

    @ ---- Advance pointers ----
    ADD     R0, R0, #16                 @ data   += 4 complex samples (16 bytes)
    ADD     R1, R1, #16                 @ window += 4 coefficients   (16 bytes)

    @ ---- Prefetch next cache lines ----
    PLD     [R0, #32]
    PLD     [R1, #32]

    @ ---- Loop control ----
    SUBS    R3, R3, #1                  @ Decrement group counter
    BNE     .Lloop4                     @ Continue until all groups done

.Lexit:
    POP     {R4-R11, PC}               @ Restore registers and return

    .size   fft_apply_window, . - fft_apply_window
    .end
