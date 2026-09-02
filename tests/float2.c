#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>


/*
 * Convert a Mitra-15 value (E,A) to double.
 *
 * E = most significant 16 bits
 * A = least significant 16 bits
 */
static double mitra_to_double(uint16_t A, uint16_t E)
{
    uint32_t raw = ((uint32_t)E << 16) | A;

    if (raw == 0)
        return 0.0;

    /*
     * Negative Mitra numbers are stored as the
     * two's complement of the complete 32-bit value.
     *
     * Convert back to the positive magnitude first.
     */
    bool negative = (raw & 0x80000000U) != 0;

    if (negative)
        raw = (~raw) + 1;

    /* Extract characteristic and mantissa */
    int C = (raw >> 24) & 0x7F;
    uint32_t mant = raw & 0xFFFFFFU;

    /*
     * M = mantissa / 2^24
     */
    double M = mant / 16777216.0;     /* 2^24 */

    /*
     * N = M * 16^(C-64)
     */
    double value = M * pow(16.0, C - 64);

    return negative ? -value : value;
}


/*
 * Convert a double to Mitra-15.
 *
 * Returns false if the value is outside the
 * representable Mitra-15 range.
 */
static bool double_to_mitra(double v, uint16_t *A, uint16_t *E)
{
    if (!A || !E)
        return false;

    /* Zero */
    if (v == 0.0) {
        *A = 0;
        *E = 0;
        return true;
    }

    bool negative = v < 0.0;
    double x = fabs(v);

    /*
     * Find e such that:
     *
     *     x = M * 16^e
     *
     * with:
     *
     *     1/16 <= M < 1
     */
    int e = (int)floor(log(x) / log(16.0)) + 1;

    double M = x / pow(16.0, e);

    /*
     * Convert M to the 24-bit integer mantissa.
     */
    uint64_t mant =
        (uint64_t)llround(M * 16777216.0);

    /*
     * Rounding can theoretically produce 2^24.
     * Renormalize in that case.
     */
    if (mant >= 16777216ULL) {
        mant = 1048576ULL;       /* 2^20 = 2^24 / 16 */
        e++;
    }

    /*
     * Mitra characteristic:
     *
     *     C = e + 64
     *
     * The specification says:
     *
     *     0 < C < 127
     */
    int C = e + 64;

    if (C <= 0 || C >= 127)
        return false;

    /*
     * Construct the positive 32-bit representation.
     */
    uint32_t raw =
        ((uint32_t)C << 24) |
        ((uint32_t)mant & 0xFFFFFFU);

    /*
     * Negative numbers are represented by the
     * two's complement of the complete positive word.
     */
    if (negative)
        raw = (~raw) + 1;

    /* Split into E and A */
    *E = (uint16_t)(raw >> 16);
    *A = (uint16_t)(raw & 0xFFFF);

    return true;
}


int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s number\n", argv[0]);
        return 1;
    }

    /* Read the command-line number */
    char *end;
    double original = strtod(argv[1], &end);

    if (*end != '\0') {
        fprintf(stderr, "Invalid number: %s\n", argv[1]);
        return 1;
    }

    /*
     * --------------------------------------------------
     * double -> Mitra-15
     * --------------------------------------------------
     */
    uint16_t A, E;

    if (!double_to_mitra(original, &A, &E)) {
        fprintf(stderr,
                "Number cannot be represented in Mitra-15 format.\n");
        return 1;
    }

    uint32_t raw =
        ((uint32_t)E << 16) | A;

    /*
     * --------------------------------------------------
     * Mitra-15 -> double
     * --------------------------------------------------
     */
    double result = mitra_to_double(A, E);

    /*
     * --------------------------------------------------
     * Print everything
     * --------------------------------------------------
     */

    printf("Original double : %.17g\n", original);

    printf("\nMitra-15 representation:\n");
    printf("  E              = 0x%04X\n", E);
    printf("  A              = 0x%04X\n", A);
    printf("  32-bit word    = 0x%08X\n", raw);

    printf("\nBack to double:\n");
    printf("  mitra_to_double = %.17g\n", result);

    printf("\nDifference:\n");
    printf("  %.17g\n", result - original);

    return 0;
}
