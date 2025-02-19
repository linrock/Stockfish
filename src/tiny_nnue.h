#pragma once

#include "simd.h"
#include "types.h"

#define TinyEvalFile "HL128-hse-1k-50.bin"

// (768 -> HL)x2 -> 1
#define HIDDEN_WIDTH   128

#define NETWORK_SCALE  340
#define NETWORK_QA     255
#define NETWORK_QB      64


using namespace Stockfish;

struct SquarePiece {
  Square sq;
  Piece pc;
};
typedef struct SquarePiece SquarePiece;

struct TinyDirtyPieces {
  SquarePiece sub0, add0, sub1, add1;

  enum {
    DP_NORMAL, DP_CAPTURE, DP_CASTLING
  } type;
};
typedef struct TinyDirtyPieces TinyDirtyPieces;

struct NNUEAccumulator {
  union {
    alignas(SIMD_ALIGNMENT) int16_t colors[COLOR_NB][HIDDEN_WIDTH];
    alignas(SIMD_ALIGNMENT) int16_t both[COLOR_NB * HIDDEN_WIDTH];
  };
};
typedef struct NNUEAccumulator NNUEAccumulator;

void nnue_init(void);
Value nnue_evaluate(const Position &pos);
