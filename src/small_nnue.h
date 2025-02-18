#pragma once

#include "simd.h"
#include "types.h"

// #define EvalFile "HL64-hm--S2-T77nov-T79-wdl-pdist-see-ge0-lr125--S1-UHO-no-wm-lr15-pdist-re2-120.nnue"
#define SmallEvalFile "HL64-qa101-qb160-S2-T77novT79maraprmay.rearranged.8-bit.nnue"

// (768 -> HL)x2 -> 1
#define HIDDEN_WIDTH    64

#define NETWORK_SCALE  340
#define NETWORK_QA     101
#define NETWORK_QB     160


using namespace Stockfish;

// (768 -> HL)x2 -> 1
// Output layer input size:
// 2x hidden for dual-perspective, 1x hidden for single-perspective
struct NNUE {
  alignas(SIMD_ALIGNMENT) int16_t featureTransformerWeights[2][6][64][HIDDEN_WIDTH];
  alignas(SIMD_ALIGNMENT) int16_t featureTransformerBiases[HIDDEN_WIDTH];
  alignas(SIMD_ALIGNMENT) int16_t outputWeights[2 * HIDDEN_WIDTH];
                          int16_t outputBias;
};
extern struct NNUE NNUE;

struct SquarePiece {
  Square sq;
  Piece pc;
};
typedef struct SquarePiece SquarePiece;

struct DirtyPieces {
  SquarePiece sub0, add0, sub1, add1;

  enum {
    DP_NORMAL, DP_CAPTURE, DP_CASTLING
  } type;
};
typedef struct DirtyPieces DirtyPieces;

struct NNUEAccumulator {
  union {
    alignas(SIMD_ALIGNMENT) int16_t colors[COLOR_NB][HIDDEN_WIDTH];
    alignas(SIMD_ALIGNMENT) int16_t both[COLOR_NB * HIDDEN_WIDTH];
  };

  bool computed[COLOR_NB];
};
typedef struct NNUEAccumulator NNUEAccumulator;

void nnue_init(void);

void nnue_accumulator_refresh(NNUEAccumulator *accumulator, const Position *pos, Color pov);

void nnue_accumulator_update(
  NNUEAccumulator *accumulator, Square whiteKingSq, Square blackKingSq, Color side, DirtyPieces* dp,
  NNUEAccumulator *prevAccumulator);

Value nnue_evaluate(NNUEAccumulator *accumulator, Color sideToMove);
