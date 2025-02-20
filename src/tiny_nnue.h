#pragma once

#include "simd.h"
#include "types.h"

#define TinyEvalFile "HL256-hse-900-1k-sc340-sb50-pdist-2b-pow32-50.bin"

// (768 -> HL)x2 -> 1
#define HIDDEN_WIDTH   256

#define NETWORK_SCALE  340
#define NETWORK_QA     255
#define NETWORK_QB      64


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
