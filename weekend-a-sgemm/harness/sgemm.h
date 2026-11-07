#ifndef SGEMM_H
#define SGEMM_H

/*
 * C = A * B for square n x n row-major float matrices.
 *
 * Contract with the harness:
 * - The kernel must write every element of C. The harness fills C with NaN
 *   once before the first call and never clears it, so a kernel that
 *   accumulates into C must zero it itself.
 * - A, B and C are 64-byte aligned and do not overlap.
 */
void sgemm(int n, const float *A, const float *B, float *C);

#endif
