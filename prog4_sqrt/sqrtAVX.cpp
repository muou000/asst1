#include <immintrin.h>
#include <math.h>

void sqrtAVX(int N,
             float initialGuess,
             float values[],
             float output[])
{
    static const float kThreshold = 0.00001f;
    __m256 v_threshold = _mm256_set1_ps(kThreshold);
    __m256 v_one = _mm256_set1_ps(1.0f);
    __m256 v_three = _mm256_set1_ps(3.0f);
    __m256 v_half = _mm256_set1_ps(0.5f);
    __m256 v_initGuess = _mm256_set1_ps(initialGuess);
    // 0x7fffffff mask to clear sign bit for fabs
    __m256 v_abs_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));

    int i = 0;
    for (; i + 8 <= N; i += 8) {
        __m256 x = _mm256_loadu_ps(&values[i]);
        __m256 guess = v_initGuess;

        // error = fabs(guess * guess * x - 1.0f)
        __m256 guess2 = _mm256_mul_ps(guess, guess);
        __m256 term = _mm256_mul_ps(guess2, x);
        __m256 diff = _mm256_sub_ps(term, v_one);
        __m256 error = _mm256_and_ps(diff, v_abs_mask);

        // mask = error > kThreshold
        __m256 mask = _mm256_cmp_ps(error, v_threshold, _CMP_GT_OQ);

        while (!_mm256_testz_ps(mask, mask)) {
            // guess = (3.f * guess - x * guess * guess * guess) * 0.5f;
            __m256 g2 = _mm256_mul_ps(guess, guess);
            __m256 xg3 = _mm256_mul_ps(_mm256_mul_ps(x, guess), g2);
            __m256 three_g = _mm256_mul_ps(v_three, guess);
            __m256 new_guess = _mm256_mul_ps(_mm256_sub_ps(three_g, xg3), v_half);

            guess = _mm256_blendv_ps(guess, new_guess, mask);

            // error = fabs(guess * guess * x - 1.0f)
            __m256 new_guess2 = _mm256_mul_ps(guess, guess);
            __m256 new_term = _mm256_mul_ps(new_guess2, x);
            __m256 new_diff = _mm256_sub_ps(new_term, v_one);
            error = _mm256_and_ps(new_diff, v_abs_mask);

            mask = _mm256_and_ps(mask, _mm256_cmp_ps(error, v_threshold, _CMP_GT_OQ));
        }

        __m256 res = _mm256_mul_ps(x, guess);
        _mm256_storeu_ps(&output[i], res);
    }

    // Scalar fallback for remaining elements
    for (; i < N; i++) {
        float x = values[i];
        float guess = initialGuess;
        float error = fabsf(guess * guess * x - 1.f);
        while (error > kThreshold) {
            guess = (3.f * guess - x * guess * guess * guess) * 0.5f;
            error = fabsf(guess * guess * x - 1.f);
        }
        output[i] = x * guess;
    }
}
