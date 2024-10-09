/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2024 The Stockfish developers (see AUTHORS file)

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

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>
#include <fstream>

#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"

namespace Stockfish {

// Returns a static, purely materialistic evaluation of the position from
// the point of view of the given color. It can be divided by PawnValue to get
// an approximation of the material advantage on the board in terms of pawns.
int Eval::simple_eval(const Position& pos, Color c) {
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c))
         + (pos.non_pawn_material(c) - pos.non_pawn_material(~c));
}

bool Eval::use_smallnet(const Position& pos) {
    int simpleEval = simple_eval(pos, pos.side_to_move());
    return std::abs(simpleEval) > 962;
}


float relu(float x) {
    return x > 0 ? x : 0;
}

float sigmoid(float x) {
    return 1 / (1 + exp(-x));
}

// Matrix-vector multiplication function
void matmul(const float weights[][9], const float inputs[], float result[], int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        result[i] = 0;
        for (int j = 0; j < cols; ++j) {
            result[i] += weights[i][j] * inputs[j];
        }
    }
}

// Matrix-vector multiplication function for other layer sizes
void matmul_other(const float weights[][16], const float inputs[], float result[], int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        result[i] = 0;
        for (int j = 0; j < cols; ++j) {
            result[i] += weights[i][j] * inputs[j];
        }
    }
}

// Add bias
void add_bias(const float outputs[], const float bias[], float result[], int size) {
    for (int i = 0; i < size; ++i) {
        result[i] = outputs[i] + bias[i];
    }
}

constexpr float weights_fc1[16][9] = {
    {-0.30492136, -0.3304863, -0.22851694, -0.059588395, 0.1837014, -0.23529534, -0.12411749, -0.19843905, -0.20490174},
    {-2.9761276, 3.078269, 2.9377608, 0.15346797, -0.010939398, 0.11419828, -0.06770054, -0.0942929, 0.17510144},
    {0.63666207, -1.4707739, -0.8232925, 0.24651326, -0.008285548, -0.15442607, -0.011370033, -0.19756411, -0.47569367},
    {0.46030304, -2.3472474, -1.9760001, -0.15377225, -0.17262185, -0.1739332, -0.42220908, -0.37911686, -0.5350044},
    {-2.7517564, 3.2356946, 2.839854, 0.06784466, 0.029544314, -0.24298453, 0.1747426, 0.094103515, 0.24222532},
    {-2.5388565, 3.374285, 2.9143577, 0.04830395, -0.0053708465, 0.18857494, -0.11250197, -0.118037276, 0.076186866},
    {4.8550735, -2.58831, -1.9171695, 0.2003081, -0.0061353846, 0.13441569, -0.05451912, -0.14757061, 0.29164314},
    {-0.5035821, 0.09510039, -2.6491542, -0.33158717, 0.029420111, 0.9440667, -3.9707274, -0.41899177, 0.049955845},
    {-0.9961981, 1.1372327, 0.26645586, 0.44523996, -0.15638056, 0.21672264, -0.17323314, -0.23829183, -0.48008156},
    {-3.6734443, 2.8381093, 2.339706, 0.13785642, 0.0137304105, 0.021938847, 0.06545986, -0.15282907, 0.3462413},
    {-0.998516, 1.6943821, 1.2896398, 0.07080595, -0.05805365, -0.020484358, -0.13121158, -0.14919202, -0.04038258},
    {2.3581698, -3.5216553, -2.5189373, 0.25359264, 0.010844275, 0.19494377, -0.06882488, -0.20949067, 0.55056494},
    {-3.0079575, 2.8011951, 2.6929371, -0.06695505, 0.027407642, -0.03692168, 0.033308353, 0.060858317, 0.48121828},
    {-2.6016803, -1.811864, -1.635785, 0.09826079, 0.0032078838, 0.17224538, -0.121252194, -0.06998718, -0.12437794},
    {-1.7549958, 1.6897104, 1.1720273, 0.102775514, -0.06956856, -0.08741274, -0.18995847, -0.14217873, 0.08051651},
    {4.743403, 0.37064958, 0.41453782, 0.008661771, 0.01006794, -0.074041426, 0.07056979, -0.021617757, 0.19382273},
};

constexpr float bias_fc1[16] = {-0.15902874, -1.2186407, 0.32761863, 1.3543808, -1.496684, -1.5443585, 0.20097277, 0.11601008, -0.38075957, -0.56900555, -1.1019036, 1.46452, -1.2129014, 2.793981, -0.7331336, -2.8346126};

constexpr float weights_fc2[16][16] = {
    {0.12606686, -2.496304, 0.0014139713, 0.12526894, -4.895492, -9.581062, 0.74838173, 0.0025555377, -0.13232403, -0.19030711, -0.05510166, 4.0549865, -0.15293089, -0.029315699, -0.028668873, -0.6979235},
    {0.105629414, 4.217032, -0.11625302, 0.6793458, 0.29442444, -0.54648906, -5.474548, 0.49416077, 0.6703477, 1.9318452, -0.7116775, 0.47473577, 1.146784, 1.840492, 0.6546536, -3.558618},
    {0.08913058, -4.2495313, 0.5328142, 0.1255907, -3.0125992, -0.4138559, 2.1086607, 1.7420702, 0.5022117, 0.3005335, -0.15373628, 1.511786, -2.0169296, 1.4372606, -0.1941049, -3.4530573},
    {-0.2045728, -0.19645427, -0.2675083, 0.18473849, 0.09844682, -0.115632124, -0.1799762, 0.07756755, 0.08557209, -0.26528287, 0.010014325, 0.079616226, -0.07313893, -0.03652511, 0.18374252, -0.1982311},
    {0.15289408, 2.5947423, -0.16349928, 0.7231507, 0.7632996, 4.14667, -0.35427764, -0.33117923, 0.080615774, 0.9225098, 0.20899518, -2.5409112, 0.58860934, -0.021466352, 0.04666779, -0.35565597},
    {-0.11668676, -0.24459758, -0.29309794, 0.17985651, -0.08191565, 0.16376033, 0.18218638, -0.07839518, 0.024008172, -0.20185462, -0.20058149, -0.24215718, 0.08214648, 0.08509602, -0.15511793, -0.03644793},
    {0.14429227, -0.35826546, 0.089762405, -0.66219735, -1.8271706, -0.4483093, 2.3432798, -0.60170925, 0.85889405, -2.2292342, 1.4542931, -1.2713969, 0.65913296, 0.25357878, 0.54880536, 4.2860785},
    {0.1635102, -4.6377196, 0.46863395, 1.43413, -2.9541106, -5.1029153, -0.072339475, 1.3877538, 0.42643744, -0.9547001, -1.8135409, 1.7175126, -5.2895985, 1.6543797, 0.31482935, 1.9321762},
    {-0.048930645, -2.5582373, 0.43805748, 0.43495575, -0.65134543, -0.9355598, -0.30781117, 1.6395704, 0.08068461, -0.41323224, -0.48596537, 1.6029552, -1.304461, 0.09157944, -1.2981306, -0.6180857},
    {0.10389191, -0.007684549, -0.21641767, -0.36153108, -0.13542911, -0.10760274, -0.04646327, -0.22428527, 0.052620586, -0.13379675, -0.11260591, -0.13923514, -0.0131347915, -0.12886497, -0.33033365, -0.3175219},
    {0.21040356, -2.425461, 0.40676004, -0.11821462, -0.9743125, 1.6530147, 1.3704501, 1.5712109, 0.64986694, -1.078962, -0.44036242, 1.7219008, -1.2570091, 1.1696438, -0.6438454, -3.3928998},
    {0.24288025, 0.13215536, -0.07702941, -0.08825654, -0.16129276, -0.01614198, -0.18636161, -0.24593592, 0.10251528, -0.0011384785, -0.11279008, -0.24364206, 0.24218535, 0.01732266, -0.11779493, -0.0824652},
    {-0.081712425, -0.012670368, -0.040042847, -0.19811818, -0.0785971, 0.009657413, -0.22303343, 0.11966255, -0.10174993, -0.14966455, 0.07542005, -0.13466832, 0.17325011, -0.07843089, 0.06706026, 0.00926441},
    {-0.083131045, 2.7483878, 0.0023745836, -0.26775357, 4.6595106, 8.617017, -1.2477064, -0.3893417, 0.116299994, 0.38219145, 0.1288836, -3.6188662, 0.41775003, -0.19261244, 0.15407869, -0.56816804},
    {0.24698082, 0.031220362, -0.10388781, 0.16044088, -0.20965396, 0.20145279, -0.23188758, 0.16619498, -0.12345208, -0.0305011, -0.19917399, -0.068579264, 0.023009613, -0.31526163, -0.009169763, -0.03228935},
    {-0.18789262, -3.0311785, 0.62459207, -0.3909912, -2.144367, -0.35191372, 1.1662662, 1.8177941, 0.29755667, -1.4557055, -0.7275326, 1.7844976, -2.3219225, 0.1017898, -1.1049712, -9.826622},
};

constexpr float bias_fc2[16] = {1.2773215, 1.846711, -0.33444235, -0.16846758, -0.84270936, -0.28752512, -0.87626004, -0.5087332, -0.15292682, 0.039221253, -0.40075484, -0.13154206, -0.22465768, -0.98320156, -0.08575093, 0.33196706};

constexpr float weights_fc3[1][16] = {
    {33.410164, -8.835699, 0.6045785, 0.09875922, 9.944291, 0.021068627, -1.71094, 0.7560074, 0.0052576223, 0.0098410845, 0.41693458, 0.13764781, 0.101195276, 34.329662, -0.0044607995, 4.1426897},
};

constexpr float bias_fc3[] = {-4.102735};

// Main inference function
float run_inference(const float input[9]) {
    // Forward pass
    // Layer 1: Input -> FC1 -> ReLU
    float out_fc1[16]; // Output from the first layer (16 neurons)
    matmul(weights_fc1, input, out_fc1, 16, 9);
    float out_fc1_biased[16];
    add_bias(out_fc1, bias_fc1, out_fc1_biased, 16);
    for (int i = 0; i < 16; ++i) {
        out_fc1_biased[i] = relu(out_fc1_biased[i]);
    }

    // Layer 2: FC1 -> FC2 -> ReLU
    float out_fc2[8]; // Output from the second layer (8 neurons)
    matmul_other(weights_fc2, out_fc1_biased, out_fc2, 8, 16);
    float out_fc2_biased[8];
    add_bias(out_fc2, bias_fc2, out_fc2_biased, 8);
    for (int i = 0; i < 8; ++i) {
        out_fc2_biased[i] = relu(out_fc2_biased[i]);
    }

    // Layer 3: FC2 -> FC3 -> Sigmoid
    float out_fc3[1]; // Output from the third layer (1 neuron)
    matmul_other(weights_fc3, out_fc2_biased, out_fc3, 1, 8);
    float out_fc3_biased[1];
    add_bias(out_fc3, bias_fc3, out_fc3_biased, 1);
    float output = sigmoid(out_fc3_biased[0]);

    return output; // This will be the probability (between 0 and 1)
}


// Evaluate is the evaluator for the outer world. It returns a static evaluation
// of the position from the point of view of the side to move.
Value Eval::evaluate(const Eval::NNUE::Networks&    networks,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());

    bool smallNet = use_smallnet(pos);
    int  v;

    auto [psqt, positional] = smallNet ? networks.small.evaluate(pos, &caches.small)
                                       : networks.big.evaluate(pos, &caches.big);

    Value nnue = (125 * psqt + 131 * positional) / 128;
    int nnueComplexity = std::abs(psqt - positional);

    // Re-evaluate the position when higher eval accuracy is worth the time spent
    // if (smallNet && (nnue * psqt < 0 || std::abs(nnue) < 227))
    if (smallNet && std::abs(nnue) < 500)
    {
        int simpleEval = simple_eval(pos, pos.side_to_move());
        float input[9] = {
          (float)std::clamp(nnue - -4730, 0, 5245 - -4730) / (5245 - -4730),
          (float)std::clamp(psqt - -5784, 0, 6030 - -5784) / (6030 - -5784),
          (float)std::clamp(positional - -4168, 0, 6274 - -4168) / (6274 - -4168),
          (float)std::clamp(nnueComplexity - 0, 0, 7222) / 7222,
          (float)std::clamp(optimism - -131, 0, 131 - -131) / (131 - -131),
          (float)std::clamp(pos.count<ALL_PIECES>() - 3, 0, 31 - 3) / (31 - 3),
          (float)std::clamp(pos.count<PAWN>() - 0, 0, 16) / 16,
          (float)std::clamp(pos.non_pawn_material() - 0, 0, 19142) / 19142,
          (float)std::clamp(simpleEval - -5915, 0, 6044 - -5915) / (6044 - -5915),
        };
        if (run_inference(input) > 0.5) {
            std::tie(psqt, positional) = networks.big.evaluate(pos, &caches.big);
            nnue                       = (125 * psqt + 131 * positional) / 128;
            smallNet                   = false;
        }
    }

    // Blend optimism and eval with nnue complexity
    nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * nnueComplexity / (smallNet ? 430 : 474);
    nnue -= nnue * nnueComplexity / (smallNet ? 20233 : 17879);

    int material = (smallNet ? 553 : 532) * pos.count<PAWN>() + pos.non_pawn_material();
    v = (nnue * (76898 + material) + optimism * (8112 + material)) / (smallNet ? 74411 : 76256);

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule50_count() / 212;

    // Guarantee evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Networks& networks) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    auto caches = std::make_unique<Eval::NNUE::AccumulatorCaches>(networks);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, networks, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    auto [psqt, positional] = networks.big.evaluate(pos, &caches->big);
    Value v                 = psqt + positional;
    v                       = pos.side_to_move() == WHITE ? v : -v;
    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)\n";

    v = evaluate(networks, pos, *caches, VALUE_ZERO);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "Final evaluation       " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)";
    ss << " [with scaled NNUE, ...]";
    ss << "\n";

    return ss.str();
}

}  // namespace Stockfish
