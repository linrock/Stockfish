#pragma once

#include "simd.h"
#include "types.h"

#define TinyEvalFile "HL128-hm--S2-T77nov-T79-wdl-pdist-see-ge0--S1-UHO-no-wm-lr15-wdl-pdist-120.bin"

// (768 -> HL)x2 -> 1
#define HIDDEN_WIDTH   128

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
