#include "bitboard.h"
#include "incbin/incbin.h"
#include "position.h"
#include "tiny_nnue.h"

#include <string.h>
#include <stdio.h>


using namespace Stockfish;

const uint8_t weightsPerSimdVector = sizeof(vector) / sizeof(int16_t);
const uint8_t numSimdCalls = HIDDEN_WIDTH / weightsPerSimdVector;

INCBIN(TinyEmbeddedNNUE, TinyEvalFile);

struct {
  alignas(SIMD_ALIGNMENT) int16_t featureTransformerWeights[COLOR_NB][6][SQUARE_NB][HIDDEN_WIDTH];
  alignas(SIMD_ALIGNMENT) int16_t featureTransformerBiases[HIDDEN_WIDTH];
  alignas(SIMD_ALIGNMENT) int16_t outputWeights[2 * HIDDEN_WIDTH];
                          int16_t outputBias;
} TinyNNUE;
// struct TinyNNUE TinyNNUE;

static inline int16_t* NNUEfeatureAddress(Square kingSq, Color pov, Piece pc, Square sq) {
  // horizontally mirror if POV king is on right side of the board (EFGH files)
  if ((kingSq % 8) > 3) {
    kingSq = (Square)((int)kingSq ^ 7);
    sq = (Square)((int)sq ^ 7);
  }
  return TinyNNUE.featureTransformerWeights[pov != color_of(pc)][type_of(pc) - 1][relative_square(pov, sq)];
}

void nnue_init() {
  memcpy(&TinyNNUE, gTinyEmbeddedNNUEData, sizeof(TinyNNUE));
  // for (int i = 0; i < 2 * HIDDEN_WIDTH; i++) {
  //   sync_cout << "outputWeights[" << i << "] = " << TinyNNUE.outputWeights[i] << sync_endl;
  // }
  // sync_cout << "outputBias: " << TinyNNUE.outputBias << sync_endl;
}

Value nnue_evaluate(const Position &pos) {
  std::unique_ptr<NNUEAccumulator> accumulator = std::make_unique<NNUEAccumulator>();

  // reset
  Color pov = pos.side_to_move();
  memcpy(accumulator->colors[pov], TinyNNUE.featureTransformerBiases, sizeof(TinyNNUE.featureTransformerBiases));

  // refresh
  Square povKingSq = pos.square<KING>(pov == WHITE ? WHITE : BLACK);
  vector* vAcc = (vector*) accumulator->colors[pov];
  for (Bitboard b = pos.pieces(); b; )
  {
    Square sq = pop_lsb(b);
    Piece pc = pos.piece_on(sq);
    for (uint8_t i = 0; i < numSimdCalls; ++i)
    {
      vector* featureToActivate = (vector*) NNUEfeatureAddress(povKingSq, pov, pc, sq);
      vAcc[i] = add_epi16(vAcc[i], featureToActivate[i]);
    }
  }

  // evaluate
  vector* ourAcc = (vector*) accumulator->colors[pov];
  vector* theirAcc = (vector*) accumulator->colors[!pov];

  vector* ourOutputWeights = (vector*) &TinyNNUE.outputWeights[0];
  vector* theirOutputWeights = (vector*) &TinyNNUE.outputWeights[HIDDEN_WIDTH];

  const vector vZero = vector_setzero();
  const vector vQA = vector_set1_epi16(NETWORK_QA);

  vector v0, v1;
  vector sum = vector_setzero();

  for (uint8_t i = 0; i < numSimdCalls; ++i) {
    // our perspective
    v0 = max_epi16(ourAcc[i], vZero);
    v0 = min_epi16(v0, vQA);
    v1 = mullo_epi16(v0, ourOutputWeights[i]);
    v1 = madd_epi16(v1, v0);
    sum = add_epi32(sum, v1);

    // their perspective
    v0 = max_epi16(theirAcc[i], vZero);
    v0 = min_epi16(v0, vQA);
    v1 = mullo_epi16(v0, theirOutputWeights[i]);
    v1 = madd_epi16(v1, v0);
    sum = add_epi32(sum, v1);
  }

  int unsquared = vector_hadd_epi32(sum) / NETWORK_QA + TinyNNUE.outputBias;

  return (Value)((unsquared * NETWORK_SCALE) / (NETWORK_QA * NETWORK_QB));
}
