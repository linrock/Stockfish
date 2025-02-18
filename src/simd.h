#pragma once

#if defined(__AVX2__)

#include <immintrin.h>

typedef __m256i vector;
#define SIMD_ALIGNMENT 32

static inline vector add_epi16(vector x, vector y) {
  return _mm256_add_epi16(x, y);
}

static inline vector add_epi32(vector x, vector y) {
  return _mm256_add_epi32(x, y);
}

static inline vector sub_epi16(vector x, vector y) {
  return _mm256_sub_epi16(x, y);
}

static inline vector min_epi16(vector x, vector y) {
  return _mm256_min_epi16(x, y);
}

static inline vector max_epi16(vector x, vector y) {
  return _mm256_max_epi16(x, y);
}

static inline vector mullo_epi16(vector x, vector y) {
  return _mm256_mullo_epi16(x, y);
}

static inline vector madd_epi16(vector x, vector y) {
  return _mm256_madd_epi16(x, y);
}

static inline vector vector_setzero(void) {
  return _mm256_setzero_si256();
}

static inline vector vector_set1_epi16(int16_t x) {
  return _mm256_set1_epi16(x);
}

static inline int vector_hadd_epi32(vector vec) {
  __m128i xmm0;
  __m128i xmm1;

  // Get the lower and upper half of the register:
  xmm0 = _mm256_castsi256_si128(vec);
  xmm1 = _mm256_extracti128_si256(vec, 1);

  // Add the lower and upper half vertically:
  xmm0 = _mm_add_epi32(xmm0, xmm1);

  // Get the upper half of the result:
  xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

  // Add the lower and upper half vertically:
  xmm0 = _mm_add_epi32(xmm0, xmm1);

  // Shuffle the result so that the lower 32-bits are directly above the second-lower 32-bits:
  xmm1 = _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(2, 3, 0, 1));

  // Add the lower 32-bits to the second-lower 32-bits vertically:
  xmm0 = _mm_add_epi32(xmm0, xmm1);

  // Cast the result to the 32-bit integer type and return it:
  return _mm_cvtsi128_si32(xmm0);
}

#else
typedef __m128i vector;
#define SIMD_ALIGNMENT 32

static inline vector add_epi16(vector x, vector y) {
  return _mm_add_epi16(x, y);
}

static inline vector add_epi32(vector x, vector y) {
  return _mm_add_epi32(x, y);
}

static inline vector sub_epi16(vector x, vector y) {
  return _mm_sub_epi16(x, y);
}

static inline vector min_epi16(vector x, vector y) {
  return _mm_min_epi16(x, y);
}

static inline vector max_epi16(vector x, vector y) {
  return _mm_max_epi16(x, y);
}

static inline vector mullo_epi16(vector x, vector y) {
  return _mm_mullo_epi16(x, y);
}

static inline vector madd_epi16(vector x, vector y) {
  return _mm_madd_epi16(x, y);
}

static inline vector vector_setzero(void) {
  return _mm_setzero_si128();
}

static inline vector vector_set1_epi16(int16_t x) {
  return _mm_set1_epi16(x);
}

static inline int vector_hadd_epi32(vector vec) {
  int* asArray = (int*) &vec;
  return asArray[0] + asArray[1] + asArray[2] + asArray[3];
}

#endif
