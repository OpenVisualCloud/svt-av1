/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file WedgeUtilTest.cc
 *
 * @brief Unit test for util functions in wedge prediction:
 * - eb_av1_wedge_sign_from_residuals_avx2
 * - eb_av1_wedge_compute_delta_squares_avx2
 * - eb_av1_wedge_sse_from_residuals_avx2
 *
 * @author Cidana-Wenyao
 *
 ******************************************************************************/
#include "gtest/gtest.h"
#include "EbDefinitions.h"
#include "EbUtility.h"
#include "aom_dsp_rtcd.h"
#include "random.h"

using svt_av1_test_tool::SVTRandom;
namespace {
// test eb_av1_wedge_sign_from_residuals_avx2
// Choose the mask sign for a compound predictor.
class WedgeUtilTest : public ::testing::Test {
  public:
    void SetUp() override {
        r0 = (int16_t *)eb_aom_memalign(32, MAX_SB_SQUARE * sizeof(int16_t));
        r1 = (int16_t *)eb_aom_memalign(32, MAX_SB_SQUARE * sizeof(int16_t));
        m = (uint8_t *)eb_aom_memalign(32, MAX_SB_SQUARE);
    }

    void TearDown() override {
        eb_aom_free(r0);
        eb_aom_free(r1);
        eb_aom_free(m);
        aom_clear_system_state();
    }

    void wedge_sign_test(int N, int k) {
        DECLARE_ALIGNED(32, int16_t, ds[MAX_SB_SQUARE]);
        // pre-compute limit
        // MAX_MASK_VALUE/2 * (sum(r0**2) - sum(r1**2))
        int64_t limit;
        limit = (int64_t)eb_aom_sum_squares_i16_c(r0, N);
        limit -= (int64_t)eb_aom_sum_squares_i16_c(r1, N);
        limit *= (1 << WEDGE_WEIGHT_BITS) / 2;

        // calculate ds: r0**2 - r1**2
        for (int i = 0; i < N; ++i)
            ds[i] = clamp(r0[i] * r0[i] - r1[i] * r1[i], INT16_MIN, INT16_MAX);
        const int8_t ref_sign =
            eb_av1_wedge_sign_from_residuals_c(ds, m, N, limit);
        const int8_t tst_sign =
            eb_av1_wedge_sign_from_residuals_avx2(ds, m, N, limit);
        ASSERT_EQ(ref_sign, tst_sign)
            << "unit test for eb_av1_wedge_sign_from_residuals_avx2 fail at "
               "iteration "
            << k;
    }

  protected:
    uint8_t *m;       /* mask */
    int16_t *r0, *r1; /* two predicted residual */
};

extern "C" uint64_t eb_aom_sum_squares_i16_c(const int16_t *src, uint32_t n);
#define MAX_MASK_VALUE (1 << WEDGE_WEIGHT_BITS)
TEST_F(WedgeUtilTest, MaskSignRandomTest) {
    const int iterations = 10000;
    SVTRandom rnd(13, true);             // max residual is 13-bit
    SVTRandom m_rnd(0, MAX_MASK_VALUE);  // [0, MAX_MASK_VALUE]
    SVTRandom n_rnd(1, 8191 / 64);       // required by assembly implementation

    for (int k = 0; k < iterations; ++k) {
        // populate the residual buffer randomly
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
            r0[i] = rnd.random();
            r1[i] = rnd.random();
            m[i] = m_rnd.random();
        }

        // N should be multiple of 64, required by
        // eb_av1_wedge_sign_from_residuals_avx2
        const int N = 64 * n_rnd.random();
        wedge_sign_test(N, k);
    }
}

TEST_F(WedgeUtilTest, MaskSignExtremeTest) {
    const int int_13bit_max = (1 << 12) - 1;
    SVTRandom rnd(13, true);             // max residual is 13-bit
    SVTRandom m_rnd(0, MAX_MASK_VALUE);  // [0, MAX_MASK_VALUE]
    SVTRandom n_rnd(1, 8191 / 64);       // required by assembly implementation

    for (int k = 0; k < 4; ++k) {
        switch (k) {
        case 0:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = 0;
                r1[i] = int_13bit_max;
            }
            break;
        case 1:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = int_13bit_max;
                r1[i] = 0;
            }
            break;
        case 2:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = -int_13bit_max;
                r1[i] = 0;
            }
            break;
        case 3:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = 0;
                r1[i] = -int_13bit_max;
            }
            break;
        default: assert(0); break;
        }

        for (int i = 0; i < MAX_SB_SQUARE; ++i)
            m[i] = MAX_MASK_VALUE;

        // N should be multiple of 64, required by
        // eb_av1_wedge_sign_from_residuals_avx2
        const int N = 64 * n_rnd.random();

        wedge_sign_test(N, k);
    }
}

// test eb_av1_wedge_compute_delta_squares_avx2
// element-by-element calculate the difference of square
TEST_F(WedgeUtilTest, ComputeDeltaSquareTest) {
    const int iterations = 10000;
    SVTRandom rnd(13, true);  // max residual is 13-bit
    SVTRandom n_rnd(1, MAX_SB_SQUARE / 64);
    DECLARE_ALIGNED(32, int16_t, ref_diff[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, int16_t, tst_diff[MAX_SB_SQUARE]);

    for (int k = 0; k < iterations; ++k) {
        // populate the residual buffer randomly
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
            r0[i] = rnd.random();
            r1[i] = rnd.random();
        }
        memset(ref_diff, 0, sizeof(ref_diff));
        memset(tst_diff, 0, sizeof(ref_diff));

        // N should be multiple of 64, required by
        // eb_av1_wedge_compute_delta_squares_avx2
        const int N = 64 * n_rnd.random();

        eb_av1_wedge_compute_delta_squares_c(ref_diff, r0, r1, N);
        eb_av1_wedge_compute_delta_squares_avx2(tst_diff, r0, r1, N);

        // check the output
        for (int i = 0; i < N; ++i) {
            ASSERT_EQ(ref_diff[i], tst_diff[i])
                << "unit test for eb_av1_wedge_compute_delta_squares_avx2 fail at "
                   "iteration "
                << k;
        }
    }
}

// test eb_av1_wedge_sse_from_residuals_avx2
// calculate the sse of two prediction combined with mask m
TEST_F(WedgeUtilTest, SseFromResidualRandomTest) {
    const int iterations = 10000;
    SVTRandom rnd(13, true);             // max residual is 13-bit
    SVTRandom m_rnd(0, MAX_MASK_VALUE);  // [0, MAX_MASK_VALUE]
    SVTRandom n_rnd(1, MAX_SB_SQUARE / 64);

    for (int k = 0; k < iterations; ++k) {
        // populate the residual buffer randomly
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
            r0[i] = rnd.random();
            r1[i] = rnd.random();
            m[i] = m_rnd.random();
        }

        // N should be multiple of 64, required by
        // eb_av1_wedge_sse_from_residuals_avx2
        const int N = 64 * n_rnd.random();

        uint64_t ref_sse = eb_av1_wedge_sse_from_residuals_c(r0, r1, m, N);
        uint64_t tst_sse = eb_av1_wedge_sse_from_residuals_avx2(r0, r1, m, N);

        // check output
        ASSERT_EQ(ref_sse, tst_sse)
            << "unit test for eb_av1_wedge_sse_from_residuals_avx2 fail at "
               "iteration "
            << k;
    }
}

TEST_F(WedgeUtilTest, SseFromResidualExtremeTest) {
    const int int_13bit_max = (1 << 12) - 1;
    SVTRandom rnd(13, true);             // max residual is 13-bit
    SVTRandom m_rnd(0, MAX_MASK_VALUE);  // [0, MAX_MASK_VALUE]
    SVTRandom n_rnd(1, MAX_SB_SQUARE / 64);

    for (int k = 0; k < 4; ++k) {
        switch (k) {
        case 0:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = 0;
                r1[i] = int_13bit_max;
            }
            break;
        case 1:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = int_13bit_max;
                r1[i] = 0;
            }
            break;
        case 2:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = -int_13bit_max;
                r1[i] = 0;
            }
            break;
        case 3:
            for (int i = 0; i < MAX_SB_SQUARE; ++i) {
                r0[i] = 0;
                r1[i] = -int_13bit_max;
            }
            break;
        default: assert(0); break;
        }

        for (int i = 0; i < MAX_SB_SQUARE; ++i)
            m[i] = MAX_MASK_VALUE;

        // N should be multiple of 64, required by
        // eb_av1_wedge_sse_from_residuals_avx2
        const int N = 64 * n_rnd.random();

        uint64_t ref_sse = eb_av1_wedge_sse_from_residuals_c(r0, r1, m, N);
        uint64_t tst_sse = eb_av1_wedge_sse_from_residuals_avx2(r0, r1, m, N);

        // check output
        ASSERT_EQ(ref_sse, tst_sse)
            << "unit test for eb_av1_wedge_sse_from_residuals_avx2 fail at "
               "iteration "
            << k;
    }
}
}  // namespace
