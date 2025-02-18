#include "bitboard.h"
#include "incbin/incbin.h"
#include "position.h"
#include "small_nnue.h"

#include <string.h>
#include <stdio.h>

using namespace Stockfish;

const uint8_t weightsPerSimdVector = sizeof(vector) / sizeof(int16_t);
const uint8_t numSimdCalls = HIDDEN_WIDTH / weightsPerSimdVector;

static inline int16_t* NNUEfeatureAddress(Square kingSq, Color pov, Piece pc, Square sq) {
  // horizontally mirror if POV king is on right side of the board (EFGH files)
  if ((kingSq % 8) > 3) {
    kingSq = (Square)((int)kingSq ^ 7);
    sq = (Square)((int)sq ^ 7);
  }
  return NNUE.featureTransformerWeights[pov != color_of(pc)][type_of(pc) - 1][relative_square(pov, sq)];
}

void nnue_accumulator_refresh(NNUEAccumulator *accumulator, const Position *pos, Color pov) {
  memcpy(accumulator->colors[pov], NNUE.featureTransformerBiases, sizeof(NNUE.featureTransformerBiases));
  Square povKingSq = pos->square<KING>(pov == WHITE ? WHITE : BLACK);
  vector* vAcc = (vector*) accumulator->colors[pov];
  for (Bitboard b = pos->pieces(); b; )
  {
    Square sq = pop_lsb(b);
    Piece pc = pos->piece_on(sq);
    for (uint8_t i = 0; i < numSimdCalls; ++i)
    {
      vector* featureToActivate = (vector*) NNUEfeatureAddress(povKingSq, pov, pc, sq);
      vAcc[i] = add_epi16(vAcc[i], featureToActivate[i]);
    }
  }
}

void nnue_accumulator_update(
  NNUEAccumulator *accumulator, Square whiteKingSq, Square blackKingSq, Color side, DirtyPieces* dp,
  NNUEAccumulator *prevAccumulator)
{
  Square povKingSq = side == WHITE ? whiteKingSq : blackKingSq;
  vector* vPrevAcc = (vector*) prevAccumulator->colors[side];
  vector* vAcc = (vector*) accumulator->colors[side];

  // printf("    DP_NORMAL sub: %d %d %d %d\n", povKingSq, side, dp->sub0.pc, dp->sub0.sq);
  // printf("    DP_NORMAL add: %d %d %d %d\n", povKingSq, side, dp->add0.pc, dp->add0.sq);
  vector* featureFromSq = (vector*) NNUEfeatureAddress(povKingSq, side, dp->sub0.pc, dp->sub0.sq);
  vector* featureToSq = (vector*) NNUEfeatureAddress(povKingSq, side, dp->add0.pc, dp->add0.sq);

  // piece move - deactivate from, activate to
  if (dp->type == DirtyPieces::DP_NORMAL)
  {
    for (uint8_t i = 0; i < numSimdCalls; ++i)
      vAcc[i] = sub_epi16(add_epi16(vPrevAcc[i], featureToSq[i]), featureFromSq[i]);
  }

  // piece capture - deactivate from, activate to, deactivate captured
  else if (dp->type == DirtyPieces::DP_CAPTURE)
  {
    vector* featureCaptured = (vector*) NNUEfeatureAddress(povKingSq, side, dp->sub1.pc, dp->sub1.sq);

    for (uint8_t i = 0; i < numSimdCalls; ++i)
      vAcc[i] = sub_epi16(sub_epi16(add_epi16(vPrevAcc[i], featureToSq[i]), featureFromSq[i]), featureCaptured[i]);
  }

  // castling - deactivate king from, activate king to, deactivate rook from, activate rook to
  else if (dp->type == DirtyPieces::DP_CASTLING)
  {
    vector* rookFromSq = (vector*) NNUEfeatureAddress(povKingSq, side, dp->sub1.pc, dp->sub1.sq);
    vector* rookToSq = (vector*) NNUEfeatureAddress(povKingSq, side, dp->add1.pc, dp->add1.sq);

    for (uint8_t i = 0; i < numSimdCalls; ++i)
      vAcc[i] = add_epi16(sub_epi16(sub_epi16(add_epi16(vPrevAcc[i], featureToSq[i]), featureFromSq[i]), rookFromSq[i]), rookToSq[i]);
  }

  // accumulator->computed[side] = true;
}

Value nnue_evaluate(NNUEAccumulator *accumulator, Color sideToMove) {
  vector* ourAcc = (vector*) accumulator->colors[sideToMove];
  vector* theirAcc = (vector*) accumulator->colors[!sideToMove];

  vector* ourOutputWeights = (vector*) &NNUE.outputWeights[0];
  vector* theirOutputWeights = (vector*) &NNUE.outputWeights[HIDDEN_WIDTH];

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

  int unsquared = vector_hadd_epi32(sum) / NETWORK_QA + NNUE.outputBias;

  return (Value)((unsquared * NETWORK_SCALE) / (NETWORK_QA * NETWORK_QB));
}
