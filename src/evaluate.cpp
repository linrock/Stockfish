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
    {0.31516707, -0.028779825, -0.05716034, -0.2225953, 0.18002968, -0.22384807, -0.16058823, 0.013181647, -0.32635057},
    {0.025755556, -0.018282255, 0.015082428, -0.3096576, -0.037705842, -0.015942749, 0.22306003, 0.05632493, -0.20500796},
    {-1.0629292, 1.2153102, 1.1387631, 0.002245056, 1.3862198e-05, -0.00097578665, -0.0012778151, 0.0013733518, 0.014376571},
    {-0.045190684, -0.27918205, -1.6280407, 0.64956135, -0.011443227, 0.027813148, -0.5002841, -0.13107683, -0.39734522},
    {-1.0334417, 0.66022813, 1.4730746, -0.026708301, -0.0714901, 0.106648706, -0.08451999, -0.12993953, 0.06567846},
    {-0.2236594, -0.1724945, -0.16723041, -0.31305176, 0.10469477, -0.15417847, -0.26619586, 0.05864068, 0.15307939},
    {-0.99661136, 1.1867849, 0.4617891, 0.45588517, -0.0124539025, -0.18306667, 0.14877455, -0.028696425, 0.18236919},
    {-0.8681748, 1.1120381, 1.3710674, -0.49449, 0.05661568, 0.12669279, -0.10491861, 0.11540815, -0.054334022},
    {-0.07668325, 0.61979634, -0.44008002, -0.6034592, 0.041644562, -2.193131, -0.6795144, -1.1504015, 0.32923463},
    {-0.1964822, -0.4613181, 0.83303994, -0.028438373, -3.4045865e-05, -1.8281306, -1.8329737, 0.28949863, 0.14947015},
    {0.58454555, -0.18595433, 0.4388255, -0.30087435, 0.014469864, -0.62777, -1.1914912, -1.5005447, -0.43946505},
    {0.08826971, 0.3028024, 1.0262091, -0.31808788, 0.061346404, 0.30890584, 0.21493724, -0.026199065, -0.24805783},
    {-0.119825624, -0.9329741, -0.6388342, 0.03250663, -1.2611399, 0.6000145, -0.2568181, -0.17126216, -0.09985406},
    {-0.30480433, 0.26424456, 0.059883993, 0.29973787, -0.29364687, 0.23634073, -0.22159708, 0.015662234, 0.02891465},
    {-0.42846748, 0.79552335, 0.48072284, 0.16157672, 0.048465826, 0.65517515, -1.82132, 0.005873272, 0.23392618},
    {0.1821699, 0.27704304, 0.52172726, -1.3009228, -0.13934715, -0.8310789, -0.16622128, -0.2707134, -0.41902512},
};

constexpr float bias_fc1[16] = {-0.20026486, -0.1517337, -0.5551984, 0.60896903, -0.16699374, -0.053001605, -0.2814143, -0.3972928, 0.046142653, -0.15466745, 0.12718609, -0.70079774, 0.49655306, -0.32496834, -0.80988806, -0.4039238};

constexpr float weights_fc2[16][16] = {
    {-0.050074458, -0.22274625, -0.052954823, -0.06776914, -0.22084582, 0.17924792, 0.0538396, -0.04413179, -0.18272653, 0.08424289, 0.07565077, -0.10151581, 0.2012125, 0.13510951, 0.07242302, -0.27150646},
    {0.0729723, -0.011377325, 0.0016518085, 0.0038016797, -0.08403224, 0.008502603, -0.18537739, -0.033668965, -0.050538797, 0.12025683, -0.16130883, -0.16508539, 0.0766229, -0.11474231, -0.20102213, -0.07476793},
    {-0.045838058, -0.062019378, 11.231891, -0.119683586, -0.91826254, 0.12458643, -1.6260097, -1.4498168, -0.07814479, 0.10754611, 0.059119325, -0.043215595, 0.43344522, 0.07548475, 0.4929509, 0.09821633},
    {-0.24030918, 0.07444961, 28.98967, 0.35826695, -0.6209731, 0.011907667, -1.0790066, -0.9567162, -0.027718376, -0.25773382, 0.030146832, -0.01844543, 0.92775404, -0.21227288, 0.31538507, -0.14613429},
    {-0.09924808, -0.107148185, 21.880688, -0.028027192, -0.7450629, -0.14058995, -1.3094467, -1.165529, -0.06837741, 0.17700212, 0.07759146, -0.027374107, 0.6169944, -0.0087687075, 0.26201424, -0.049929395},
    {0.1761367, -0.25331146, -13.9665575, -6.1853695, 0.47748682, 0.11162725, -0.5215504, -0.3040393, 0.6762459, -1.3606315, -0.6721634, -1.5543491, -0.13965997, 0.032707214, -6.142245, -4.30407},
    {0.104398936, -0.16496229, -6.9553137, -0.8327059, -6.482678, 0.0017100871, 0.1846189, -3.7588887, -3.2480283, -2.0925417, -0.47031966, -3.031445, -1.6170245, 0.13700998, 1.4008768, -0.27816692},
    {0.06101677, 0.18530875, -0.11663866, -0.16366461, -0.029831463, 0.10025272, -0.00862933, -0.09410172, -0.1291477, -0.18674001, 0.12510175, -0.21089157, -0.28851357, -0.20053905, -0.10925863, -0.31798807},
    {0.21317667, -0.08762407, -0.15829806, 0.04575851, -0.12278475, -0.14793858, -0.08421766, -0.1112852, -0.070150815, -0.1309286, 0.095622, 0.074765876, -0.1858598, 0.15732172, 0.061307013, -0.10660661},
    {-0.15794972, 0.0029536642, 23.091034, 0.30652034, -0.72456855, -0.019846648, -1.2845411, -1.1358314, -0.049648065, -0.18115878, 0.034178473, -0.02473669, 1.4270741, 0.15715078, 0.6588871, 0.0032714996},
    {0.008961976, -0.046679735, 0.067628145, -0.10430485, 0.24123684, 0.24391592, -0.11164272, -0.24901125, 0.07040596, -0.015792996, 0.20751736, -0.1065869, 0.046499997, -0.019689083, 0.17275491, -0.1676698},
    {-0.10675886, -0.097196475, -0.20799649, -0.14252606, -0.21541533, -0.013004094, -0.22211027, -0.28666136, -0.19241312, -0.2946227, -0.30411026, -0.10716564, 0.065900184, -0.15329036, 0.091790594, 0.071969084},
    {0.23681045, -0.038740933, -0.23698094, -0.06931123, -0.17028517, 0.06782076, 0.00912872, -0.24150997, -0.10351971, 0.18218732, 0.06660992, 0.15725273, 0.028600037, 0.16602382, 0.0999549, -0.099458605},
    {0.0793384, 0.12275847, 15.831712, -0.18018284, -0.86115986, 0.08892459, -1.5315982, -1.3532764, -0.112141825, 0.14219593, 0.07113658, -0.050210852, 0.50276214, 0.1514251, 0.26095292, 0.08117259},
    {0.08023718, -0.24720255, -0.041649908, -0.14823392, -0.3420663, 0.13596666, -0.35484022, -0.22472163, -0.12184684, -0.20196965, 0.10970871, 0.046716213, -0.08153935, -0.19289088, -0.1423442, -0.009344985},
    {-0.09383193, -0.18007849, 16.17416, -0.18673576, -0.83983594, 0.10958198, -1.4828703, -1.3081052, -0.112620935, 0.14453423, 0.08418828, -0.050253063, 0.5184474, 0.053246707, 0.38402304, 0.06158027},
};

constexpr float bias_fc2[16] = {-0.16015668, -0.035243843, 0.7935934, 0.5280826, 0.6391037, 0.2605143, -0.10212284, -0.059846077, -0.088331245, 0.6237813, -0.21488616, 0.018698635, -0.17413947, 0.7442191, 0.047496114, 0.72183335};

constexpr float weights_fc3[1][16] = {
    {-0.08958817, -0.06951866, 17.45558, 17.177202, 18.314512, -3.05234, -2.1935499, -0.062030014, -0.09143891, 17.161058, 0.23373935, -0.0939805, -0.12666574, 18.95326, -0.043434024, 17.96006},
};

constexpr float bias_fc3[] = {0.41877022};

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

    float result = 0;
    if (smallNet) {
        int simpleEval = simple_eval(pos, pos.side_to_move());
        // std::vector<float> input = {0.5, 0.3, 0.7, 0.1, 0.9, 0.4, 0.2, 0.6, 0.8, 0.3};
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

        /*
        sync_cout << "input: " << sync_endl;
        for (float val : input) {
            sync_cout << val << " " << sync_endl;
        }
        */

        result = run_inference(input);
        // sync_cout << "Inference result: " << result << sync_endl;
        // dbg_hit_on(result > 0.5, 0);
        // dbg_hit_on((nnue * psqt < 0 || std::abs(nnue) < 227), 1);
    }

    // Re-evaluate the position when higher eval accuracy is worth the time spent
    // if (smallNet && (nnue * psqt < 0 || std::abs(nnue) < 227))
    if (smallNet && result > 0.5)
    {
        std::tie(psqt, positional) = networks.big.evaluate(pos, &caches.big);
        nnue                       = (125 * psqt + 131 * positional) / 128;
        smallNet                   = false;
    }

    // Blend optimism and eval with nnue complexity
    nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * nnueComplexity / (smallNet ? 430 : 474);
    nnue -= nnue * nnueComplexity / (smallNet ? 20233 : 17879);

    int material = (smallNet ? 553 : 532) * pos.count<PAWN>() + pos.non_pawn_material();
    v = (nnue * (76898 + material) + optimism * (8112 + material)) / (smallNet ? 74411 : 76256);

    // Evaluation grain (to get more alpha-beta cuts) with randomization (for robustness)
    v = (v / 16) * 16 - 1 + (pos.key() & 0x2);

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
