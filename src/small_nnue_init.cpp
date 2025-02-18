#include "bitboard.h"
#include "incbin/incbin.h"
#include "position.h"
#include "small_nnue.h"

#include <string.h>
#include <stdio.h>

using namespace Stockfish;

// This macro invocation will declare the following three variables
//     const unsigned char        gEmbeddedNNUEData[];  // a pointer to the embedded data
//     const unsigned char *const gEmbeddedNNUEEnd;     // a marker to the end
//     const unsigned int         gEmbeddedNNUESize;    // the size of the embedded file
INCBIN(EmbeddedNNUE, SmallEvalFile);
//     const unsigned char        gEmbeddedNNUEData[1]  = {0x0};
//     const unsigned char* const gEmbeddedNNUEEnd      = &gEmbeddedNNUEData[1];
//     const unsigned int         gEmbeddedNNUESize     = 1;

static int16_t map_unused_8bit_to_actual_16bit_weight(int piece_type, int pov, int8_t value) {
  int16_t m[6][2][256];
  memset(m, 0, sizeof(m));

  // our pawn
  m[0][0][0] = -200;
  m[0][0][1] = -196;
  m[0][0][2] = -148;
  m[0][0][3] = -142;
  // their pawn
  m[0][1][0] = -191;
  m[0][1][1] = -175;
  m[0][1][3] = -171;
  m[0][1][4] = -164;
  m[0][1][5] = -163;
  m[0][1][6] = -160;
  m[0][1][7] = -157;
  m[0][1][8] = -141;
  m[0][1][9] = -139;

  // our knight
  m[1][0][0] = -131;
  // their knight


  // our bishop
  m[2][0][0] = -142;
  m[2][0][1] = -136;
  m[2][0][2] = -135;
  m[2][0][3] = -129;
  // their bishop


  // our rook

  // their rook


  // our queen
  m[4][0][0] = -182;
  m[4][0][1] = -181;
  m[4][0][2] = -175;
  m[4][0][3] = -173;
  m[4][0][4] = -172;
  m[4][0][5] = -170;
  m[4][0][6] = -167;
  m[4][0][7] = -166;
  m[4][0][8] = -165;
  m[4][0][9] = -164;
  m[4][0][10] = -163;
  m[4][0][11] = -162;
  m[4][0][12] = -161;
  m[4][0][13] = -160;
  m[4][0][14] = -159;
  m[4][0][15] = -158;
  m[4][0][16] = -157;
  m[4][0][17] = -156;
  m[4][0][18] = -155;
  m[4][0][19] = -154;
  m[4][0][20] = -153;
  m[4][0][21] = -152;
  m[4][0][22] = -151;
  m[4][0][23] = -150;
  m[4][0][24] = -149;
  m[4][0][25] = -148;
  m[4][0][26] = -146;
  m[4][0][27] = -145;
  m[4][0][28] = -144;
  m[4][0][29] = -143;
  m[4][0][30] = -142;
  m[4][0][31] = -141;
  m[4][0][32] = -140;
  m[4][0][33] = -139;
  m[4][0][34] = -138;
  // their queen
  m[4][1][0] = -155;
  m[4][1][1] = -154;
  m[4][1][2] = -144;
  m[4][1][3] = -139;
  m[4][1][16] = -138;
  m[4][1][249] = 132;
  m[4][1][247] = 129;
  m[4][1][246] = 128;

  // our king

  // their king


  int index = (int)value + 128;
  if (index >= 0 && index < 256) {
    int16_t mapped_value = m[piece_type][pov][index];
    if (mapped_value != 0) {  // assuming 0 means no mapping
      // printf("m[%d][%d][%d] = %d\n", piece_type, pov, index, m[piece_type][pov][index]);
      // printf("mapped value 8-bit to 16-bit: %d -> %d\n", value, mapped_value);
      return mapped_value;
    }
  }

  // no mapping exists, so return the original value as 16-bit
  // printf("no mapping found for m[%d][%d][%d]. value is: %d\n", piece_type, pov, index, value);
  return (int16_t)value;
}

struct NNUE NNUE;

void nnue_init(void) {
  const int8_t *nnue_data_8bit = (const int8_t *)gEmbeddedNNUEData;
  size_t data_size = gEmbeddedNNUESize;

  // printf("sizeof(gEmbeddedNNUEData): %d\n", sizeof(gEmbeddedNNUEData));
  // printf("gEmbeddedNNUESize: %d\n", data_size);

  // const PieceType pts[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

  int idx;
  for (int pov = 0; pov < COLOR_NB; pov++)
  {
    for (int pt = 0; pt < 6; pt++)   // pawn, knight, bishop, rook, queen, king
    {
      for (int i = 0; i < HIDDEN_WIDTH; i++)
      {
        for (int sq = SQ_A1; sq < SQUARE_NB; sq++)
        {
          // int16_t featureTransformerWeights[2][6][64][HIDDEN_WIDTH];
          // printf("NNUE.featureTransformerWeights[%d][%d][%d][%d] = ...\n", pov, pt-1, sq, i);
          // printf("8-bit value: %d\n", nnue_data_8bit[idx + i]);

          idx =  COLOR_NB * pt * SQUARE_NB * HIDDEN_WIDTH
               + pov * SQUARE_NB * HIDDEN_WIDTH
               + i * SQUARE_NB
               + sq;

          /*
          if (sq == 0) {
            printf("NNUE.featureTransformerWeights[%d][%d][%d][%d] = %d\n", pov, pt, sq, i, nnue_data_8bit[idx]);
          }
          */

          // temp_weights[sq][i] = map_unused_8bit_to_actual_16bit_weight(pt-1, pov, nnue_data_8bit[idx]);
          NNUE.featureTransformerWeights[pov][pt][sq][i] = 
            map_unused_8bit_to_actual_16bit_weight(pt, pov, nnue_data_8bit[idx]);
        }
      }
    }
  }

  /*
  for (int pt = 0; pt < 6; pt++)
  {
    for (int pov = 0; pov < 2; pov++)
    {
      for (Square sq = SQ_A1; sq < SQUARE_NB; sq++)
      {
        printf("FeatureWeights[%d][%d][%d] = {", pt, pov, sq);
        for (int i = 0; i < HIDDEN_WIDTH; i++)
        {
          printf("%d", NNUE.featureTransformerWeights[pov][pt][sq][i]);
          if (i < HIDDEN_WIDTH-1) printf(", ");
        }
        printf("};\n");
      }
      printf("\n");
    }
  }
  */

  idx = 6 * COLOR_NB * SQUARE_NB * HIDDEN_WIDTH;
  // printf("idx: %d\n", idx);
  // printf("data size: %d\n", data_size);

  // printf("FT biases: %d\n", HIDDEN_WIDTH);
  // printf("ftB = {");
  for (int i = 0; i < HIDDEN_WIDTH; i++) {
    NNUE.featureTransformerBiases[i] = (int16_t)(nnue_data_8bit[idx + i]);
    // printf("%d", (int16_t)(nnue_data_8bit[idx + i]));
    // if (i < HIDDEN_WIDTH-1) printf(", ");
  }
  // printf("};\n\n");
  idx += HIDDEN_WIDTH;

  // printf("output weights: %d\n", 2 * HIDDEN_WIDTH);
  // printf("oW = {");
  for (int i = 0; i < 2 * 2 * HIDDEN_WIDTH; i += 2) {
    NNUE.outputWeights[i / 2] =
      (int16_t)((uint8_t)nnue_data_8bit[idx + i] | ((uint8_t)nnue_data_8bit[idx + i + 1] << 8));
    // printf("%d", NNUE.outputWeights[i / 2]);
    // if (i < 4 * HIDDEN_WIDTH - 2) printf(", ");
  }
  // printf("};\n\n");
  idx += 4 * HIDDEN_WIDTH;

  // oB = -237;
  NNUE.outputBias = (int16_t)(nnue_data_8bit[data_size-2] | (nnue_data_8bit[data_size-1] << 8));
  // printf("NNUE.outputBias: %d\n", NNUE.outputBias);

  // memcpy(&NNUE, gEmbeddedNNUEData, sizeof(NNUE));
  // printf("sizeof(gEmbeddedNNUEData): %d\n", gEmbeddedNNUEData);
  // printf("gEmbeddedNNUESize): %d\n", gEmbeddedNNUESize);

  // const Color povs[] = {WHITE, BLACK};

  // struct {
  //   alignas(SIMD_ALIGNMENT) int16_t featureTransformerWeights[2][6][64][HIDDEN_WIDTH];
  //   alignas(SIMD_ALIGNMENT) int16_t featureTransformerBiases[HIDDEN_WIDTH];
  //   alignas(SIMD_ALIGNMENT) int16_t outputWeights[2 * HIDDEN_WIDTH];
  //                          int16_t outputBias;
  // } NNUE;

  /*
  for (int i = 0; i < HIDDEN_WIDTH; i++) {
    printf("feature_biases[%d] = %d;\n", i, NNUE.featureTransformerBiases[i]);
  }
  printf("\n");

  for (int i = 0; i < 2 * HIDDEN_WIDTH; i++) {
    printf("output_weights[%d] = %d;\n", i, NNUE.outputWeights[i]);
  }
  printf("\n");
  printf("output_bias = %d;\n", NNUE.outputBias);
  */
}
