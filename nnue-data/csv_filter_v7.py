import io
import os.path
from pprint import pprint
import sys
import textwrap

import zstandard

''' Iterate over positions .csv files and output .plain files
'''
if len(sys.argv) != 2:
    print('Usage: ./csv_filter_v7.py <input_csv_file>')
    sys.exit(0)

input_filename = sys.argv[1]
if input_filename.endswith(".csv"):
    output_filename = input_filename.replace('.csv', '.csv.filter-v7.plain')
elif input_filename.endswith(".csv.zst"):
    output_filename = input_filename.replace('.csv.zst', '.csv.zst.filter-v7.plain')

if os.path.isfile(output_filename):
    print(f'Found .csv.zst.filter-v7.plain file, doing nothing:')
    print(output_filename)
    sys.exit(0)
elif os.path.isfile(output_filename.replace('.csv.zst.filter-v7.plain', '.csv.zst.filter-v7.binpack')):
    print(f'Found .csv.zst.filter-v7.binpack file, doing nothing:')
    print(output_filename.replace('.csv.zst.filter-v7.plain', '.csv.zst.filter-v7.binpack'))
    sys.exit(0)


EARLY_PLY_SKIP = 28


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
        self.num_only_one_move = 0
        self.num_one_good_move_v6 = 0
        self.num_one_good_move_v7 = 0
        self.num_positions_filtered_out = 0

    def process_csv_rows(self):
        positions = []
        prev_ply = -1
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
            if ply < prev_ply:
                # assume this is the start of the next training game in the dataset
                if fen.startswith('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq'):
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
            # skip if there's only one good move in the position (best two moves score diff is high enough)
            elif abs(sf_bestmove1_score) < 100 and abs(sf_bestmove2_score) > 150:
                # best move about equal, 2nd best move loses
                self.num_one_good_move_v6 += 1
                should_filter_out = True
            elif abs(sf_bestmove1_score) > 150 and abs(sf_bestmove2_score) < 100:
                # best move gains advantage, 2nd best move equalizes
                self.num_one_good_move_v6 += 1
                should_filter_out = True
            elif abs(sf_bestmove1_score) < 80 and abs(sf_bestmove2_score) > 120:
                # best move about equal, 2nd best move loses
                self.num_one_good_move_v7 += 1
                should_filter_out = True
            elif abs(sf_bestmove1_score) > 120 and abs(sf_bestmove2_score) < 80:
                # best move gains advantage, 2nd best move equalizes
                self.num_one_good_move_v7 += 1
                should_filter_out = True
            # if the 2 best moves favor different sides
            elif (sf_bestmove1_score > 0) != (sf_bestmove2_score > 0):
                pv2_score_diff = abs(sf_bestmove1_score - sf_bestmove2_score)
                # remove positions where the pv2 score difference is high enough
                if pv2_score_diff > 200:
                    self.num_one_good_move_v6 += 1
                    should_filter_out = True
                elif pv2_score_diff > 120:
                    self.num_one_good_move_v7 += 1
                    should_filter_out = True
            self.num_positions += 1
            prev_ply = ply
            if should_filter_out:
                self.num_positions_filtered_out += 1
            else:
                positions.append({
                    'fen': fen,
                    'move': bestmove_uci,
                    'score': bestmove_score,
                    'ply': ply,
                    'result': game_result,
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
        if self.num_positions % 100000 != 0:
            return
        num_positions_after_filter = self.num_positions - self.num_positions_filtered_out
        print(textwrap.dedent(f'''
            Processed {self.num_positions} positions
              # games:                       {self.num_games:8d}
                # standard games:            {self.num_standard_games:8d}
                # non-standard games:        {self.num_non_standard_games:8d}
              # positions:                   {self.num_positions:8d}
                # startpos:                  {self.num_start_positions:8d}
                # early plies <= 28:         {self.num_early_plies:8d}
                # only one move:             {self.num_only_one_move:8d}
                # one good move v6:          {self.num_one_good_move_v6:8d}
                # one good move v7:          {self.num_one_good_move_v7:8d}
              # positions after filtering:   {num_positions_after_filter:8d}
                % positions kept:            {num_positions_after_filter/self.num_positions*100:8.1f}
        '''))


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
