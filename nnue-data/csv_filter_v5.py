import io
import os.path
from pprint import pprint
import sys
import textwrap

import chess
import zstandard

''' Iterate over positions .csv files and output .plain files
'''
if len(sys.argv) != 2:
    print('Usage: ./csv_filter_v5.py <input_csv_file>')
    sys.exit(0)

input_filename = sys.argv[1]
if input_filename.endswith(".csv"):
    output_filename = input_filename.replace('.csv', '.csv.filter-v5.plain')
elif input_filename.endswith(".csv.zst"):
    output_filename = input_filename.replace('.csv.zst', '.csv.zst.filter-v5.plain')

if os.path.isfile(output_filename):
    print(f'Found .csv.zst.filter-v5.plain file, doing nothing:')
    print(output_filename)
    sys.exit(0)
elif os.path.isfile(output_filename.replace('.csv.zst.filter-v5.plain', '.csv.zst.filter-v5.binpack')):
    print(f'Found .csv.zst.filter-v5.binpack file, doing nothing:')
    print(output_filename.replace('.csv.zst.filter-v5.plain', '.csv.zst.filter-v5.binpack'))
    sys.exit(0)


EARLY_PLY_SKIP = 28

def move_is_promo(uci_move):
    return len(uci_move) == 5 and uci_move[-1] in ['n','b','r','q']


class PositionCsvIterator:
    def __init__(self, infile, outfile):
        self.infile = infile
        self.outfile = outfile

        self.num_games = 0
        self.num_standard_games = 0
        self.num_non_standard_games = 0

        self.num_positions = 0
        self.num_start_positions = 0
        self.num_early_plies = 0
        self.num_in_check = 0
        self.num_bestmove_captures = 0
        self.num_bestmove_promos = 0
        self.num_sf_bestmove1_captures = 0
        self.num_one_good_move = 0
        self.num_positions_filtered_out = 0

        # v5 additions
        self.num_only_one_move = 0
        self.num_obvious_draw = 0
        self.num_score_too_high = 0
        self.num_score_mismatch = 0
        self.num_large_sf_score_gap = 0

    def process_csv_rows(self):
        positions = []
        for row in self.infile:
            split_row = row.strip().split(",")
            if len(split_row) == 10:
                ply, fen, bestmove_uci, bestmove_score, game_result, \
                sf_search_method, sf_bestmove1_uci, sf_bestmove1_score, \
                sf_bestmove2_uci, sf_bestmove2_score = \
                    split_row
            elif len(split_row) == 8:
                # only one possible move in the position
                self.num_only_one_move += 1
                self.num_positions += 1
                self.num_positions_filtered_out += 1
                continue
            ply = int(ply)
            bestmove_score = int(bestmove_score)
            sf_bestmove1_score = int(sf_bestmove1_score)
            sf_bestmove2_score = int(sf_bestmove2_score)
            should_filter_out = False
            if ply == 0:
                if 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq' in fen:
                    self.num_standard_games += 1
                else:
                    self.num_non_standard_games += 1
                self.num_games += 1
                self.num_start_positions += 1
                should_filter_out = True
                if len(positions):
                    # finished processing a game. time to save it to a file
                    self.write_positions_to_file(positions)
                    positions = []
            elif ply <= EARLY_PLY_SKIP:
                # skip if an early ply position
                self.num_early_plies += 1
                should_filter_out = True
            elif abs(bestmove_score) > 9000:
                self.num_score_too_high += 1
                should_filter_out = True
            elif bestmove_score == 0 and abs(sf_bestmove1_score) <= 1 and abs(sf_bestmove2_score) <= 1:
                # skip if the best two moves are an obvious draw
                self.num_obvious_draw += 1
                should_filter_out = True
            else:
                # skip if there's only one good move in the position (best two moves score diff is high enough)
                if abs(sf_bestmove1_score) < 100 and abs(sf_bestmove2_score) > 150:
                    # best move about equal, 2nd best move loses
                    self.num_one_good_move += 1
                    should_filter_out = True
                elif abs(sf_bestmove1_score) > 150 and abs(sf_bestmove2_score) < 100:
                    # best move gains advantage, 2nd best move equalizes
                    self.num_one_good_move += 1
                    should_filter_out = True
                elif (abs(sf_bestmove1_score) > 150 and abs(sf_bestmove2_score) > 150) and \
                     (sf_bestmove1_score > 0) != (sf_bestmove2_score > 0):
                    # best move gains an advantage, 2nd best move loses
                    self.num_one_good_move += 1
                    should_filter_out = True
                # skip if the score gap between bestmoves 1 and 2 is large
                elif abs(sf_bestmove1_score - sf_bestmove2_score) > 500:
                    self.num_large_sf_score_gap += 1
                    should_filter_out = True
                # skip if the leela score is a lot different from SF search score
                elif abs(bestmove_score - sf_bestmove1_score) > 500:
                    # best move gains an advantage, 2nd best move loses
                    self.num_score_mismatch += 1
                    should_filter_out = True
            # else:
                # b = chess.Board(fen)
                # if b.is_check():
                      # skip if in check
                #     self.num_in_check += 1
                #     should_filter_out = True
                # else:
                    # filter out if provided move is a capture or promo
                    # bestmove = chess.Move.from_uci(bestmove_uci)
                    # if b.is_capture(bestmove):
                    #     self.num_bestmove_captures += 1
                    #     should_filter_out = True
                    # elif move_is_promo(bestmove_uci):
                    #     self.num_bestmove_promos += 1
                    #     should_filter_out = True
            self.num_positions += 1
            if should_filter_out:
                self.num_positions_filtered_out += 1
            else:
                positions.append({
                    'fen': fen,
                    'move': bestmove_uci,
                    'score': bestmove_score,
                    'ply': ply,
                    'result': game_result,
                    'should_filter_out': should_filter_out,
                })
            self.print_stats()
        self.print_stats()

    def write_positions_to_file(self, positions):
        game_plain = ''
        for position in positions:
            game_plain += textwrap.dedent(f'''
                fen {position['fen']}
                score {position['score']}
                move {str(position['move'])}
                ply {position['ply']}
                result {position['result']}
                e''')
        self.outfile.write(game_plain.strip() + "\n")

    def print_stats(self):
        if self.num_positions % 10000 != 0:
            return
        num_positions_after_filter = self.num_positions - self.num_positions_filtered_out
        print(f'Processed {self.num_positions} positions')
        print(f'  # games:                       {self.num_games:8d}')
        print(f'    # standard games:            {self.num_standard_games:8d}')
        print(f'    # non-standard games:        {self.num_non_standard_games:8d}')
        print(f'  # positions:                   {self.num_positions:8d}')
        print(f'    # startpos:                  {self.num_start_positions:8d}')
        print(f'    # early plies <= 28:         {self.num_early_plies:8d}')
        print(f'    # in check:                  {self.num_in_check:8d}')
        print(f'    # bestmove captures:         {self.num_bestmove_captures:8d}')
        print(f'    # bestmove promos:           {self.num_bestmove_promos:8d}')
        print(f'    # sf bestmove1 cap promo:    {self.num_sf_bestmove1_captures:8d}')
        print(f'    # one good move:             {self.num_one_good_move:8d}')
        print(f'    # obvious draw:              {self.num_obvious_draw:8d}')
        print(f'    # score diff too high:       {self.num_score_too_high:8d}')
        print(f'    # leela / sf score mismatch: {self.num_score_mismatch:8d}')
        print(f'    # bm 1,2 large score gap:    {self.num_large_sf_score_gap:8d}')
        print(f'    # only one move:             {self.num_only_one_move:8d}')
        print(f'  # positions after filtering:   {num_positions_after_filter:8d}')
        print(f'    % positions kept:            {num_positions_after_filter/self.num_positions*100:8.1f}')


print(f'Processing {input_filename} ...')
if input_filename.endswith(".csv.zst"):
    with open(input_filename, 'rb') as compressed_infile, \
         open(output_filename, 'w+') as outfile:
        dctx = zstandard.ZstdDecompressor()
        stream_reader = dctx.stream_reader(compressed_infile)
        text_stream = io.TextIOWrapper(stream_reader, encoding='utf-8')
        PositionCsvIterator(text_stream, outfile).process_csv_rows()
else:
    with open(input_filename, 'r') as infile, \
         open(output_filename, 'w+') as outfile:
        PositionCsvIterator(infile, outfile).process_csv_rows()
print(f'Saved to {output_filename}')
