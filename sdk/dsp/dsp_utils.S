;; @file dsp_utils.S
;; @brief Assembly utility routines for mmWave Radar DSP (ARM Cortex-R4/R5)
;; @details
;;   - Complex magnitude approximation (alpha-max-beta-min)
;;   - Complex multiply (Q15 fixed-point)
;;   - Vector dot product (16-bit signed)

    .syntax unified
    .arch   armv7-r
    .text
    .align  5

;; --------------------------------------------------------------------------
;; void dsp_cmag_approx(const int16_t* in_i, const int16_t* in_q,
;;                      uint16_t* out_mag, uint32_t num_samples)
;;   R0 = in_i, R1 = in_q, R2 = out_mag, R3 = num_samples
;;   Alpha-max plus beta-min:  mag ~ 0.9604*max + 0.3978*min
;;   Q8 constants: alpha=246, beta=102
;; --------------------------------------------------------------------------
    .global dsp_cmag_approx
dsp_cmag_approx:
    PUSH    {R4-R8, LR}
    CMP     R3, #0
    BEQ     .Lcmag_done

.Lcmag_loop:
    LDRSH   R4, [R0], #2               @ R4 = I sample (sign-extended)
    LDRSH   R5, [R1], #2               @ R5 = Q sample

    @ Absolute values
    CMP     R4, #0
    RSBLT   R4, R4, #0                 @ R4 = |I|
    CMP     R5, #0
    RSBLT   R5, R5, #0                 @ R5 = |Q|

    @ Determine max and min
    CMP     R4, R5
    MOVGE   R6, R4                     @ R6 = max(|I|, |Q|)
    MOVGE   R7, R5                     @ R7 = min(|I|, |Q|)
    MOVLT   R6, R5
    MOVLT   R7, R4

    @ mag = (alpha * max + beta * min) >> 8
    MOV     R8, #246                   @ alpha in Q8
    MUL     R6, R6, R8                 @ R6 = alpha * max
    MOV     R8, #102                   @ beta in Q8
    MLA     R6, R7, R8, R6             @ R6 += beta * min
    LSR     R6, R6, #8                 @ Scale back from Q8

    STRH    R6, [R2], #2               @ Store uint16 magnitude
    SUBS    R3, R3, #1
    BNE     .Lcmag_loop

.Lcmag_done:
    POP     {R4-R8, PC}

;; --------------------------------------------------------------------------
;; void dsp_cmul_q15(const int16_t* a_i, const int16_t* a_q,
;;                   const int16_t* b_i, const int16_t* b_q,
;;                   int16_t* out_i, int16_t* out_q, uint32_t n)
;;   Complex multiply: (aI+jaQ)*(bI+jbQ) = (aI*bI - aQ*bQ) + j(aI*bQ + aQ*bI)
;;   Inputs/outputs in Q15 fixed-point.
;;   R0=a_i, R1=a_q, R2=b_i, R3=b_q, stack: out_i, out_q, n
;; --------------------------------------------------------------------------
    .global dsp_cmul_q15
dsp_cmul_q15:
    PUSH    {R4-R11, LR}
    LDR     R10, [SP, #36]             @ out_i (5th arg on stack)
    LDR     R11, [SP, #40]             @ out_q (6th arg on stack)
    LDR     R9,  [SP, #44]             @ n     (7th arg on stack)
    CMP     R9, #0
    BEQ     .Lcmul_done

.Lcmul_loop:
    LDRSH   R4, [R0], #2               @ aI
    LDRSH   R5, [R1], #2               @ aQ
    LDRSH   R6, [R2], #2               @ bI
    LDRSH   R7, [R3], #2               @ bQ

    @ Real part: aI*bI - aQ*bQ
    SMULBB  R8, R4, R6                 @ R8 = aI * bI
    SMULBB  LR, R5, R7                 @ LR = aQ * bQ
    SUB     R8, R8, LR                 @ R8 = aI*bI - aQ*bQ
    ASR     R8, R8, #15                @ Q15 -> Q0
    SSAT    R8, #16, R8                @ Saturate to 16-bit
    STRH    R8, [R10], #2

    @ Imaginary part: aI*bQ + aQ*bI
    SMULBB  R8, R4, R7                 @ R8 = aI * bQ
    SMLABB  R8, R5, R6, R8             @ R8 += aQ * bI
    ASR     R8, R8, #15
    SSAT    R8, #16, R8
    STRH    R8, [R11], #2

    SUBS    R9, R9, #1
    BNE     .Lcmul_loop

.Lcmul_done:
    POP     {R4-R11, PC}

;; --------------------------------------------------------------------------
;; int32_t dsp_dot_product(const int16_t* vec_a, const int16_t* vec_b, uint32_t n)
;;   Returns sum(vec_a[i] * vec_b[i]) as 32-bit signed accumulator.
;;   R0 = vec_a, R1 = vec_b, R2 = n
;;   Uses SMLAD for dual 16-bit multiply-accumulate where possible.
;; --------------------------------------------------------------------------
    .global dsp_dot_product
dsp_dot_product:
    PUSH    {R4-R7, LR}
    MOV     R3, #0                     @ R3 = accumulator (low)

    @ Process pairs using SMLAD (dual signed multiply accumulate)
    LSR     R4, R2, #1                 @ R4 = n / 2 (pair count)
    AND     R5, R2, #1                 @ R5 = remainder (0 or 1)
    CMP     R4, #0
    BEQ     .Ldot_remainder

.Ldot_pair:
    LDR     R6, [R0], #4               @ R6 = {a[i+1], a[i]} packed
    LDR     R7, [R1], #4               @ R7 = {b[i+1], b[i]} packed
    SMLAD   R3, R6, R7, R3             @ R3 += a[i]*b[i] + a[i+1]*b[i+1]
    SUBS    R4, R4, #1
    BNE     .Ldot_pair

.Ldot_remainder:
    CMP     R5, #0
    BEQ     .Ldot_done
    LDRSH   R6, [R0]
    LDRSH   R7, [R1]
    MLA     R3, R6, R7, R3             @ accumulator += last element

.Ldot_done:
    MOV     R0, R3                     @ Return value in R0
    POP     {R4-R7, PC}

    .size   dsp_cmag_approx, . - dsp_cmag_approx
    .end
