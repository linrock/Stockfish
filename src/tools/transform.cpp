#include "transform.h"

#include "sfen_stream.h"
#include "packed_sfen.h"
#include "sfen_writer.h"

#include "thread.h"
#include "position.h"
#include "evaluate.h"
#include "search.h"

#include "nnue/evaluate_nnue.h"
#include "../extra/nnue_data_binpack_format.h"

#include <string>
#include <map>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>

namespace Stockfish::Tools
{
    using CommandFunc = void(*)(std::istringstream&);

    enum struct NudgedStaticMode
    {
        Absolute,
        Relative,
        Interpolate
    };

    struct NudgedStaticParams
    {
        std::string input_filename = "in.binpack";
        std::string output_filename = "out.binpack";
        NudgedStaticMode mode = NudgedStaticMode::Absolute;
        int absolute_nudge = 5;
        float relative_nudge = 0.1;
        float interpolate_nudge = 0.1;

        void enforce_constraints()
        {
            relative_nudge = std::max(relative_nudge, 0.0f);
            absolute_nudge = std::max(absolute_nudge, 0);
        }
    };

    struct RescoreParams
    {
        std::string input_filename = "in.epd";
        std::string output_filename = "out.binpack";
        int depth = 3;
        int filter_depth = 6;
        int filter_multipv = 2;
        int research_count = 0;
        bool keep_moves = true;
        bool debug_print = false;

        void enforce_constraints()
        {
            depth = std::max(1, depth);
            research_count = std::max(0, research_count);
        }
    };

    [[nodiscard]] std::int16_t nudge(NudgedStaticParams& params, std::int16_t static_eval_i16, std::int16_t deep_eval_i16)
    {
        auto saturate_i32_to_i16 = [](int v) {
            return static_cast<std::int16_t>(
                std::clamp(
                    v,
                    (int)std::numeric_limits<std::int16_t>::min(),
                    (int)std::numeric_limits<std::int16_t>::max()
                )
            );
        };

        auto saturate_f32_to_i16 = [saturate_i32_to_i16](float v) {
            return saturate_i32_to_i16((int)v);
        };

        int static_eval = static_eval_i16;
        int deep_eval = deep_eval_i16;

        switch(params.mode)
        {
            case NudgedStaticMode::Absolute:
                return saturate_i32_to_i16(
                    static_eval + std::clamp(
                        deep_eval - static_eval,
                        -params.absolute_nudge,
                        params.absolute_nudge
                    )
                );

            case NudgedStaticMode::Relative:
                return saturate_f32_to_i16(
                    (float)static_eval * std::clamp(
                        (float)deep_eval / (float)static_eval,
                        (1.0f - params.relative_nudge),
                        (1.0f + params.relative_nudge)
                    )
                );

            case NudgedStaticMode::Interpolate:
                return saturate_f32_to_i16(
                    (float)static_eval * (1.0f - params.interpolate_nudge)
                    + (float)deep_eval * params.interpolate_nudge
                );

            default:
                assert(false);
                return 0;
        }
    }

    void do_nudged_static(NudgedStaticParams& params)
    {
        Thread* th = Threads.main();
        Position& pos = th->rootPos;
        StateInfo si;

        auto in = Tools::open_sfen_input_file(params.input_filename);
        auto out = Tools::create_new_sfen_output(params.output_filename);

        if (in == nullptr)
        {
            std::cerr << "Invalid input file type.\n";
            return;
        }

        if (out == nullptr)
        {
            std::cerr << "Invalid output file type.\n";
            return;
        }

        PSVector buffer;
        uint64_t batch_size = 1'000'000;

        buffer.reserve(batch_size);

        const bool frc = Options["UCI_Chess960"];
        uint64_t num_processed = 0;
        for (;;)
        {
            auto v = in->next();
            if (!v.has_value())
                break;

            auto& ps = v.value();

            pos.set_from_packed_sfen(ps.sfen, &si, th, frc);
            auto static_eval = Eval::evaluate(pos);
            auto deep_eval = ps.score;
            ps.score = nudge(params, static_eval, deep_eval);

            buffer.emplace_back(ps);
            if (buffer.size() >= batch_size)
            {
                num_processed += buffer.size();

                out->write(buffer);
                buffer.clear();

                std::cout << "Processed " << num_processed << " positions. whoa\n";
            }
        }

        if (!buffer.empty())
        {
            num_processed += buffer.size();

            out->write(buffer);
            buffer.clear();

            std::cout << "Processed " << num_processed << " positions. nice\n";
        }

        std::cout << "Finished.\n";
    }

    void nudged_static(std::istringstream& is)
    {
        NudgedStaticParams params{};

        while(true)
        {
            std::string token;
            is >> token;

            if (token == "")
                break;

            if (token == "absolute")
            {
                params.mode = NudgedStaticMode::Absolute;
                is >> params.absolute_nudge;
            }
            else if (token == "relative")
            {
                params.mode = NudgedStaticMode::Relative;
                is >> params.relative_nudge;
            }
            else if (token == "interpolate")
            {
                params.mode = NudgedStaticMode::Interpolate;
                is >> params.interpolate_nudge;
            }
            else if (token == "input_file")
                is >> params.input_filename;
            else if (token == "output_file")
                is >> params.output_filename;
            else
            {
                std::cout << "ERROR: Unknown option " << token << ". Exiting...\n";
                return;
            }
        }

        std::cout << "Performing transform nudged_static with parameters:\n";
        std::cout << "input_file          : " << params.input_filename << '\n';
        std::cout << "output_file         : " << params.output_filename << '\n';
        std::cout << "\n";
        if (params.mode == NudgedStaticMode::Absolute)
        {
            std::cout << "mode                : absolute\n";
            std::cout << "absolute_nudge      : " << params.absolute_nudge << '\n';
        }
        else if (params.mode == NudgedStaticMode::Relative)
        {
            std::cout << "mode                : relative\n";
            std::cout << "relative_nudge      : " << params.relative_nudge << '\n';
        }
        else if (params.mode == NudgedStaticMode::Interpolate)
        {
            std::cout << "mode                : interpolate\n";
            std::cout << "interpolate_nudge   : " << params.interpolate_nudge << '\n';
        }
        std::cout << '\n';

        params.enforce_constraints();
        do_nudged_static(params);
    }

    void do_rescore_epd(RescoreParams& params)
    {
        std::ifstream fens_file(params.input_filename);

        auto next_fen = [&fens_file, mutex = std::mutex{}]() mutable -> std::optional<std::string>{
            std::string fen;

            std::unique_lock lock(mutex);

            if (std::getline(fens_file, fen) && fen.size() >= 10)
            {
                return fen;
            }
            else
            {
                return std::nullopt;
            }
        };

        PSVector buffer;
        uint64_t batch_size = 10'000;

        buffer.reserve(batch_size);

        auto out = Tools::create_new_sfen_output(params.output_filename);

        std::mutex mutex;
        uint64_t num_processed = 0;

        // About Search::Limits
        // Be careful because this member variable is global and affects other threads.
        auto& limits = Search::Limits;

        // Make the search equivalent to the "go infinite" command. (Because it is troublesome if time management is done)
        limits.infinite = true;

        // Since PV is an obstacle when displayed, erase it.
        limits.silent = true;

        // If you use this, it will be compared with the accumulated nodes of each thread. Therefore, do not use it.
        limits.nodes = 0;

        // depth is also processed by the one passed as an argument of Tools::search().
        limits.depth = 0;

        Threads.execute_with_workers([&](auto& th){
            Position& pos = th.rootPos;
            StateInfo si;
            const bool frc = Options["UCI_Chess960"];

            for(;;)
            {
                auto fen = next_fen();
                if (!fen.has_value())
                    return;

                pos.set(*fen, frc, &si, &th);
                pos.state()->rule50 = 0;


                for (int cnt = 0; cnt < params.research_count; ++cnt)
                    Search::search(pos, params.depth, 1);

                auto [search_value, search_pv] = Search::search(pos, params.depth, 1);

                if (search_pv.empty())
                    continue;

                PackedSfenValue ps;
                pos.sfen_pack(ps.sfen, pos.is_chess960());
                ps.score = search_value;
                ps.move = search_pv[0];
                ps.gamePly = 1;
                ps.game_result = 0;
                ps.padding = 0;

                std::unique_lock lock(mutex);
                buffer.emplace_back(ps);
                if (buffer.size() >= batch_size)
                {
                    num_processed += buffer.size();

                    out->write(buffer);
                    buffer.clear();

                    std::cout << "Processed " << num_processed << " positions.\n";
                }
            }
        });
        Threads.wait_for_workers_finished();

        if (!buffer.empty())
        {
            num_processed += buffer.size();

            out->write(buffer);
            buffer.clear();

            std::cout << "Processed " << num_processed << " positions.\n";
        }

        std::cout << "Finished.\n";
    }

    void do_rescore_data(RescoreParams& params)
    {
        // TODO: Use SfenReader once it works correctly in sequential mode. See issue #271
        auto in = Tools::open_sfen_input_file(params.input_filename);
        auto readsome = [&in, mutex = std::mutex{}](int n) mutable -> PSVector {

            PSVector psv;
            psv.reserve(n);

            std::unique_lock lock(mutex);

            for (int i = 0; i < n; ++i)
            {
                auto ps_opt = in->next();
                if (ps_opt.has_value())
                {
                    psv.emplace_back(*ps_opt);
                }
                else
                {
                    break;
                }
            }

            return psv;
        };

        auto sfen_format = ends_with(params.output_filename, ".binpack") ? SfenOutputType::Binpack : SfenOutputType::Bin;

        auto out = SfenWriter(
            params.output_filename,
            Threads.size(),
            std::numeric_limits<std::uint64_t>::max(),
            sfen_format);

        // About Search::Limits
        // Be careful because this member variable is global and affects other threads.
        auto& limits = Search::Limits;

        // Make the search equivalent to the "go infinite" command. (Because it is troublesome if time management is done)
        limits.infinite = true;

        // Since PV is an obstacle when displayed, erase it.
        limits.silent = true;

        // If you use this, it will be compared with the accumulated nodes of each thread. Therefore, do not use it.
        limits.nodes = 0;

        // depth is also processed by the one passed as an argument of Tools::search().
        limits.depth = 0;

        std::atomic<std::uint64_t> num_processed = 0;
        std::atomic<std::uint64_t> num_position_in_check = 0;
        std::atomic<std::uint64_t> num_move_already_is_capture = 0;
        std::atomic<std::uint64_t> num_capture_or_promo_skipped = 0;
        std::atomic<std::uint64_t> num_capture_or_promo_skipped_d7_multipv0 = 0;
        std::atomic<std::uint64_t> num_capture_or_promo_skipped_d7_multipv1 = 0;
        std::atomic<std::uint64_t> num_capture_or_promo_skipped_d7_multipv_eval_diff = 0;
        std::atomic<std::uint64_t> num_start_positions = 0;
        std::atomic<std::uint64_t> num_early_plies = 0;

        auto print_stats = [&]() {
            auto p = num_processed.load();
            if (p % 10000 == 0) {
                auto s = num_capture_or_promo_skipped.load();
                auto a = num_move_already_is_capture.load();
                auto c = num_position_in_check.load();
                auto st = num_start_positions.load();
                auto ep = num_early_plies.load();

                auto multipv_cap0 = num_capture_or_promo_skipped_d7_multipv0.load();
                auto multipv_cap1 = num_capture_or_promo_skipped_d7_multipv1.load();
                auto multipv_eval_diff = num_capture_or_promo_skipped_d7_multipv_eval_diff.load();

                sync_cout << "Processed " << p << " positions. Skipped " << (s+st+ep) << " positions." << sync_endl
                          << "  Static filter: " << (a+c+st+ep)
                          << " (capture: " << a << ", in check: " << c << ", start pos: " << st << ", " << "early ply: " << ep << ")"
                          << sync_endl
                          << "  MultiPV filter: " << (multipv_cap0+multipv_cap1)
                          << " (cap0: " << multipv_cap0 << ", cap1: " << multipv_cap1 << ", eval diff: " << multipv_eval_diff << ")"
                          << " depth " << params.filter_depth << sync_endl;
            }
        };

        Threads.execute_with_workers([&](auto& th){
            Position& pos = th.rootPos;
            StateInfo si;
            const bool frc = Options["UCI_Chess960"];

            const bool debug_print = params.debug_print;  // false;
            for (;;)
            {
                PSVector psv = readsome(5000);
                if (psv.empty())
                    break;

                for(auto& ps : psv)
                {
                    pos.set_from_packed_sfen(ps.sfen, &si, &th, frc);
                    // sync_cout << pos.fen() << sync_endl;

                    print_stats();
                    if (pos.checkers()) {
                        // Skip if in check
                        if (debug_print) {
                            sync_cout << "[debug] " << pos.fen() << sync_endl
                                      << "[debug] Position is in check" << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        num_capture_or_promo_skipped.fetch_add(1);
                        num_position_in_check.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                      } else if (pos.capture_or_promotion((Stockfish::Move)ps.move)) {
                        // Skip if the written move is already a capture or promotion
                        if (debug_print) {
                            sync_cout << "[debug] " << pos.fen() << sync_endl
                                      << "[debug] Provided move is capture: "
                                      << UCI::move((Stockfish::Move)ps.move, false)
                                      << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        num_capture_or_promo_skipped.fetch_add(1);
                        num_move_already_is_capture.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                    } else if (pos.fen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
                        num_start_positions.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                    } else if (pos.game_ply() <= 24) {
                        num_early_plies.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                    }

                    // use multipv search to get a sense of whether the position is tactical or quiet
                    auto [search_val, pvs] = Search::search(pos, params.filter_depth, params.filter_multipv);
                    if (pvs.empty())
                        continue;
                    if (th.rootMoves.size() == 0)
                        continue;
                    if (debug_print) {
                        sync_cout << "[debug] " << pos.fen() << sync_endl;
                        sync_cout << "[debug] Main PV move:    "
                                  << UCI::move(th.rootMoves[0].pv[0], false) << " "
                                  << th.rootMoves[0].score << " " << sync_endl;
                        if (th.rootMoves.size() > 1) {
                            sync_cout << "[debug] 2nd PV move:     "
                                      << UCI::move(th.rootMoves[1].pv[0], false) << " "
                                      << th.rootMoves[1].score << " " << sync_endl;
                        } else {
                            sync_cout << "[debug] The only valid move" << sync_endl;
                        }
                    }
                    auto best_move = th.rootMoves[0].pv[0];
                    bool more_than_one_valid_move = th.rootMoves.size() > 1;
                    if (pos.capture_or_promotion(best_move)) {
                        // skip if multipv 1st line bestmove is a capture or promo
                        num_capture_or_promo_skipped.fetch_add(1);
                        num_capture_or_promo_skipped_d7_multipv0.fetch_add(1);
                        num_processed.fetch_add(1);
                        if (debug_print) {
                            sync_cout << "[debug] Move is capture: " << UCI::move(best_move, false)
                                      << sync_endl
                                      << "[debug] 1st best move at depth 7 multipv 2" << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        continue;
                    } else if (more_than_one_valid_move && pos.capture_or_promotion(th.rootMoves[1].pv[0])) {
                        // skip if multipv 2nd line bestmove is a capture or promo
                        num_capture_or_promo_skipped.fetch_add(1);
                        num_capture_or_promo_skipped_d7_multipv1.fetch_add(1);
                        num_processed.fetch_add(1);
                        if (debug_print) {
                            sync_cout << "[debug] Move is capture: " << UCI::move(best_move, false)
                                      << sync_endl
                                      << "[debug] 2nd best move at depth 7 multipv 2" << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        continue;
                    } else if (more_than_one_valid_move) {
                      // remove positions with only 1 good move
                      Value m1_score = th.rootMoves[0].score;
                      Value m2_score = th.rootMoves[1].score;
                      if (abs(m1_score) < 110 && abs(m2_score) > 290) {
                        if (debug_print) {
                            sync_cout << "[debug] best move is about equal, 2nd best move is losing"
                                      << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        num_capture_or_promo_skipped_d7_multipv_eval_diff.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                      } else if (abs(m1_score) > 300 && abs(m2_score) < 100) {
                        if (debug_print) {
                            sync_cout << "[debug] best move gains advantage, 2nd best move equalizes"
                                      << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        num_capture_or_promo_skipped_d7_multipv_eval_diff.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                      } else if (abs(m1_score) > 300 &&
                                 (abs(m2_score) > 300 && ((m1_score > 0) != (m2_score > 0)))) {
                        if (debug_print) {
                            sync_cout << "[debug] best move gains an advantage, 2nd best move loses "
                                      << sync_endl
                                      << "[debug]" << sync_endl;
                        }
                        num_capture_or_promo_skipped_d7_multipv_eval_diff.fetch_add(1);
                        num_processed.fetch_add(1);
                        continue;
                      }
                    }

                    // only write the position if:
                    // - position is not in check
                    // - the provided move was not a capture
                    // - neither bestmove at depth7 multipv2 search is a capture
                    // - only one good move according to depth7 multipv2 search
                    pos.sfen_pack(ps.sfen, false);

                    // Don't change the score
                    // ps.score = search_value9;

                    // if (!params.keep_moves)
                    // Don't change the move
                    // ps.move = search_pv9[0];
                    ps.padding = 0;

                    out.write(th.id(), ps);

                    num_processed.fetch_add(1);
                }
            }
        });
        Threads.wait_for_workers_finished();

        std::cout << "Finished.\n";
    }

    void do_rescore(RescoreParams& params)
    {
        if (ends_with(params.input_filename, ".epd"))
        {
            do_rescore_epd(params);
        }
        else if (ends_with(params.input_filename, ".bin") || ends_with(params.input_filename, ".binpack"))
        {
            do_rescore_data(params);
        }
        else
        {
            std::cerr << "Invalid input file type.\n";
        }
    }

    void rescore(std::istringstream& is)
    {
        RescoreParams params{};

        while(true)
        {
            std::string token;
            is >> token;

            if (token == "")
                break;

            if (token == "depth")
                is >> params.depth;
            else if (token == "filter_depth")
                is >> params.filter_depth;
            else if (token == "filter_multipv")
                is >> params.filter_multipv;
            else if (token == "input_file")
                is >> params.input_filename;
            else if (token == "output_file")
                is >> params.output_filename;
            else if (token == "keep_moves")
                is >> params.keep_moves;
            else if (token == "research_count")
                is >> params.research_count;
            else if (token == "debug_print")
                is >> params.debug_print;
            else
            {
                std::cout << "ERROR: Unknown option " << token << ". Exiting...\n";
                return;
            }
        }

        params.enforce_constraints();

        std::cout << "Performing transform filter with parameters:\n";
        std::cout << "filter_depth        : " << params.filter_depth << '\n';
        std::cout << "filter_multipv      : " << params.filter_multipv << '\n';
        std::cout << "input_file          : " << params.input_filename << '\n';
        std::cout << "output_file         : " << params.output_filename << '\n';
        std::cout << "debug_print         : " << params.debug_print << '\n';
        std::cout << '\n';

        do_rescore(params);
    }

    void transform(std::istringstream& is)
    {
        const std::map<std::string, CommandFunc> subcommands = {
            { "nudged_static", &nudged_static },
            { "rescore", &rescore }
        };

        Eval::NNUE::init();

        std::string subcommand;
        is >> subcommand;

        auto func = subcommands.find(subcommand);
        if (func == subcommands.end())
        {
            std::cout << "Invalid subcommand " << subcommand << ". Exiting...\n";
            return;
        }

        func->second(is);
    }

}
