/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2023 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "psqt.h"

#include <algorithm>

#include "bitboard.h"
#include "types.h"

namespace Stockfish {

namespace
{

// 'Bonus' contains Piece-Square parameters.
// Scores are explicit for files A to D, implicitly mirrored for E to H.
constexpr Score Bonus[][RANK_NB][int(FILE_NB) / 2] = {
  { },
  { },
  { // Knight
   { -96, -65, -49, -21 },
   { -67, -54, -18, 8 },
   { -40, -27, -8, 29 },
   { -35, -2, 13, 28 },
   { -45, -16, 9, 39 },
   { -51, -44, -16, 17 },
   { -69, -50, -51, 12 },
   { -100, -88, -56, -17 }
  },
  { // Bishop
   { -40, -21, -26, -8 },
   { -26, -9, -12, 1 },
   { -11, -1, -1, 7 },
   { -14, -4, 0, 12 },
   { -12, -1, -10, 11 },
   { -21, 4, 3, 4 },
   { -22, -14, -1, 1 },
   { -32, -29, -26, -17 }
  },
  { // Rook
   { -9, -13, -10, -9 },
   { -12, -9, -1, -2 },
   { 6, -8, -2, -6 },
   { -6, 1, -9, 7 },
   { -5, 8, 7, -6 },
   { 6, 1, -7, 10 },
   { 4, 5, 20, -5 },
   { 18, 0, 19, 13 }
  },
  { // Queen
   { -69, -57, -47, -26 },
   { -54, -31, -22, -4 },
   { -39, -18, -9, 3 },
   { -23, -3, 13, 24 },
   { -29, -6, 9, 21 },
   { -38, -18, -11, 1 },
   { -50, -27, -24, -8 },
   { -74, -52, -43, -34 }
  },
  { // King
   { 1, 45, 85, 76 },
   { 53, 100, 133, 135 },
   { 88, 130, 169, 175 },
   { 103, 156, 172, 172 },
   { 96, 166, 199, 199 },
   { 92, 172, 184, 191 },
   { 47, 121, 116, 131 },
   { 11, 59, 73, 78 }
  }
};

constexpr Score PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
   { -8, -6, 9, 5, 16, 6, -6, -18 },
   { -9, -7, -10, 5, 2, 3, -8, -5 },
   { 7, 1, -8, -2, -14, -13, -11, -6 },
   { 12, 6, 2, -6, -5, -4, 14, 9 },
   { 27, 18, 19, 29, 30, 9, 8, 14 },
   { -1, -14, 13, 22, 24, 17, 7, 7 }
  };
} // namespace


namespace PSQT
{

Score psq[PIECE_NB][SQUARE_NB];

// PSQT::init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] and PBonus[], adding the piece value, then the black halves of
// the tables are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc : {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING})
  {
    Score score = PieceValue[pc];

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
      File f = File(edge_distance(file_of(s)));
      psq[ pc][s] = score + (type_of(pc) == PAWN ? PBonus[rank_of(s)][file_of(s)]
                                                 : Bonus[pc][rank_of(s)][f]);
      psq[~pc][flip_rank(s)] = -psq[pc][s];
    }
  }
}

} // namespace PSQT

} // namespace Stockfish
