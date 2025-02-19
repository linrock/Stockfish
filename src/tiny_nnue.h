#pragma once

#include "simd.h"
#include "types.h"

#define TinyEvalFile "HL128-hse-900-1k-50.bin"

// (768 -> HL)x2 -> 1
#define HIDDEN_WIDTH   128

#define NETWORK_SCALE  340
#define NETWORK_QA     512
#define NETWORK_QB      32


using namespace Stockfish;

struct NNUEAccumulator {
  union {
    alignas(SIMD_ALIGNMENT) int16_t colors[COLOR_NB][HIDDEN_WIDTH];
    alignas(SIMD_ALIGNMENT) int16_t both[COLOR_NB * HIDDEN_WIDTH];
  };
};
typedef struct NNUEAccumulator NNUEAccumulator;

void nnue_init(void);
Value nnue_evaluate(const Position &pos);
