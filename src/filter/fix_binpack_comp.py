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
    print('Usage: ./iterate_csv.py <input_csv_file>')
    sys.exit(0)

input_filename = sys.argv[1]
if input_filename.endswith(".csv"):
    output_filename = input_filename.replace('.csv', '.csv.plain')
elif input_filename.endswith(".csv.zst"):
    output_filename = input_filename.replace('.csv.zst', '.csv.zst.filter-v3.plain')

if os.path.isfile(output_filename):
    print(f'Found .csv.plain file. Doing nothing: {output_filename}')
    sys.exit(0)

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
        self.num_sf_bestmove2_captures = 0
        self.num_one_good_move = 0
        self.num_positions_filtered_out = 0

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
                ply, fen, bestmove_uci, bestmove_score, game_result, \
                sf_search_method, sf_bestmove1_uci, sf_bestmove1_score = \
                    split_row
                sf_bestmove2_uci, sf_bestmove2_score = None, None
            ply = int(ply)
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
                    # finished a game. time to compact it
                    pprint(positions)
                    for i in range(len(positions) - 1):
                        b = chess.Board(positions[i]['fen'])
                        # TODO check given moves first
                        # print(f'{b.legal_moves.count()} legal moves')
                        for move in b.legal_moves:
                            b.push(move)
                            if b.fen() == positions[i + 1]['fen']:
                                # print(f'move {move} fixes binpack!')
                                positions[i]['move'] = move
                                if positions[i]['should_filter_out']:
                                    positions[i]['score'] = 32002
                                break
                            else:
                                b.pop()
                    pprint(positions)
                    game_plain = ''
                    for position in positions:
                        game_plain += textwrap.dedent(f'''
                            fen {position['fen']}
                            score {position['score']}
                            move {str(position['move'])}
                            ply {position['ply']}
                            result {position['result']}
                            e''')
                    print(game_plain)
                    self.outfile.write(game_plain)
                    sys.exit(0)
            elif ply <= 28:
                # skip if an early ply position
                self.num_early_plies += 1
                should_filter_out = True
            else:
                b = chess.Board(fen)
                if b.is_check():
                    # skip if in check
                    self.num_in_check += 1
                    should_filter_out = True
                else:
                    # filter out if provided move is a capture or promo
                    bestmove = chess.Move.from_uci(bestmove_uci)
                    if b.is_capture(bestmove):
                        self.num_bestmove_captures += 1
                        should_filter_out = True
                    elif move_is_promo(bestmove_uci):
                        self.num_bestmove_promos += 1
                        should_filter_out = True
                    else:
                        # check if moves from SF search are captures or promos
                        sf_bestmove1 = chess.Move.from_uci(sf_bestmove1_uci)
                        if b.is_capture(sf_bestmove1) or move_is_promo(sf_bestmove1_uci):
                            # skip if SF search best move is a capture
                            self.num_sf_bestmove1_captures += 1
                            should_filter_out = True
                        elif sf_bestmove2_score:
                            # otherwise skip if the score difference is high enough
                            sf_bestmove1_score = int(sf_bestmove1_score)
                            sf_bestmove2_score = int(sf_bestmove2_score)
                            if abs(sf_bestmove1_score) < 110 and abs(sf_bestmove2_score) > 200:
                                # best move about equal, 2nd best move loses
                                self.num_one_good_move += 1
                                should_filter_out = True
                            elif abs(sf_bestmove1_score) > 200 and abs(sf_bestmove2_score) < 110:
                                # best move gains advantage, 2nd best move equalizes
                                self.num_one_good_move += 1
                                should_filter_out = True
                            elif abs(sf_bestmove1_score) > 200 and (abs(sf_bestmove2_score) > 200 and \
                                 (sf_bestmove1_score > 0) != (sf_bestmove2_score > 0)):
                                # best move gains an advantage, 2nd best move loses
                                self.num_one_good_move += 1
                                should_filter_out = True
            if should_filter_out:
                self.num_positions_filtered_out += 1
            self.num_positions += 1
            positions.append({
                'fen': fen,
                'move': bestmove_uci,
                'score': bestmove_score,
                'ply': ply,
                'result': game_result,
                'should_filter_out': should_filter_out,
            })
            self.print_stats()

    def print_stats(self):
        if self.num_positions % 10000 != 0:
            return
        num_positions_after_filter = self.num_positions - self.num_positions_filtered_out
        print(f'Processed {self.num_positions} positions')
        print(f'  # games:                       {self.num_games:8d}')
        print(f'    # standard games:            {self.num_standard_games:8d}')
        print(f'    # non-standard games:        {self.num_non_standard_games:8d}')
        print(f'  # positions:                   {self.num_positions:8d}')
        print(f'    # bestmove captures:         {self.num_bestmove_captures:8d}')
        print(f'    # bestmove promos:           {self.num_bestmove_promos:8d}')
        print(f'    # sf bestmove1 cap promo:    {self.num_sf_bestmove1_captures:8d}')
        print(f'    # sf bestmove2 cap promo:    {self.num_sf_bestmove2_captures:8d}')
        print(f'    # one good move:             {self.num_one_good_move:8d}')
        print(f'  # positions after filtering:   {num_positions_after_filter:8d}')
        print(f'    % positions kept:            {num_positions_after_filter/num_positions*100:8.1f}')


position = None
num_games = 0
num_positions = 0
num_positions_filtered_out = 0

num_start_positions = 0
num_early_plies = 0
num_in_check = 0
num_bestmove_captures = 0
num_bestmove_promos = 0
num_sf_bestmove1_captures = 0
num_sf_bestmove2_captures = 0
num_one_good_move = 0

num_standard_games = 0
num_non_standard_games = 0

def move_is_promo(uci_move):
    return len(uci_move) == 5 and uci_move[-1] in ['n','b','r','q']

def process_csv_rows(infile, outfile):
    global num_games, num_positions, num_positions_filtered_out, \
           num_bestmove_captures, num_bestmove_promos, \
           num_sf_bestmove1_captures, num_sf_bestmove2_captures, \
           num_standard_games, num_non_standard_games, \
           num_start_positions, num_early_plies, num_in_check, \
           num_one_good_move
    positions = []
    for row in infile:
        split_row = row.strip().split(",")
        if len(split_row) == 10:
            ply, fen, bestmove_uci, bestmove_score, game_result, \
            sf_search_method, sf_bestmove1_uci, sf_bestmove1_score, \
            sf_bestmove2_uci, sf_bestmove2_score = \
                split_row
        elif len(split_row) == 8:
            ply, fen, bestmove_uci, bestmove_score, game_result, \
            sf_search_method, sf_bestmove1_uci, sf_bestmove1_score = \
                split_row
            sf_bestmove2_uci, sf_bestmove2_score = None, None
        ply = int(ply)
        should_filter_out = False
        if ply == 0:
            if 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq' in fen:
                num_standard_games += 1
            else:
                num_non_standard_games += 1
            num_games += 1
            num_start_positions += 1
            should_filter_out = True
            if len(positions):
                # finished a game. time to compact it
                pprint(positions)
                for i in range(len(positions) - 1):
                    b = chess.Board(positions[i]['fen'])
                    # print(f'{b.legal_moves.count()} legal moves')
                    for move in b.legal_moves:
                        b.push(move)
                        if b.fen() == positions[i + 1]['fen']:
                            # print(f'move {move} fixes binpack!')
                            positions[i]['move'] = move
                            if positions[i]['should_filter_out']:
                                positions[i]['score'] = 32002
                            break
                        else:
                            b.pop()
                pprint(positions)
                game_plain = ''
                for position in positions:
                    game_plain += textwrap.dedent(f'''
                        fen {position['fen']}
                        score {position['score']}
                        move {str(position['move'])}
                        ply {position['ply']}
                        result {position['result']}
                        e''')
                print(game_plain)
                outfile.write(game_plain)
                sys.exit(0)
        elif ply <= 28:
            # skip if an early ply position
            num_early_plies += 1
            should_filter_out = True
        else:
            b = chess.Board(fen)
            if b.is_check():
                # skip if in check
                num_in_check += 1
                should_filter_out = True
            else:
                # filter out if provided move is a capture or promo
                bestmove = chess.Move.from_uci(bestmove_uci)
                if b.is_capture(bestmove):
                    num_bestmove_captures += 1
                    should_filter_out = True
                elif move_is_promo(bestmove_uci):
                    num_bestmove_promos += 1
                    should_filter_out = True
                else:
                    # check if moves from SF search are captures or promos
                    sf_bestmove1 = chess.Move.from_uci(sf_bestmove1_uci)
                    if b.is_capture(sf_bestmove1) or move_is_promo(sf_bestmove1_uci):
                        # skip if SF search best move is a capture
                        num_sf_bestmove1_captures += 1
                        should_filter_out = True
                    elif sf_bestmove2_score:
                        # otherwise skip if the score difference is high enough
                        sf_bestmove1_score = int(sf_bestmove1_score)
                        sf_bestmove2_score = int(sf_bestmove2_score)
                        if abs(sf_bestmove1_score) < 110 and abs(sf_bestmove2_score) > 200:
                            # best move about equal, 2nd best move loses
                            num_one_good_move += 1
                            should_filter_out = True
                        elif abs(sf_bestmove1_score) > 200 and abs(sf_bestmove2_score) < 110:
                            # best move gains advantage, 2nd best move equalizes
                            num_one_good_move += 1
                            should_filter_out = True
                        elif abs(sf_bestmove1_score) > 200 and (abs(sf_bestmove2_score) > 200 and \
                             (sf_bestmove1_score > 0) != (sf_bestmove2_score > 0)):
                            # best move gains an advantage, 2nd best move loses
                            num_one_good_move += 1
                            should_filter_out = True
        if should_filter_out:
            num_positions_filtered_out += 1
        num_positions += 1
        positions.append({
            'fen': fen,
            'move': bestmove_uci,
            'score': bestmove_score,
            'ply': ply,
            'result': game_result,
            'should_filter_out': should_filter_out,
        })
        if num_positions % 10000 == 0:
            num_positions_after_filter = num_positions - num_positions_filtered_out
            print(f'Processed {num_positions} positions')
            print(f'  # games:                       {num_games:8d}')
            print(f'    # standard games:            {num_standard_games:8d}')
            print(f'    # non-standard games:        {num_non_standard_games:8d}')
            print(f'  # positions:                   {num_positions:8d}')
            print(f'    # bestmove captures:         {num_bestmove_captures:8d}')
            print(f'    # bestmove promos:           {num_bestmove_promos:8d}')
            print(f'    # sf bestmove1 cap promo:    {num_sf_bestmove1_captures:8d}')
            print(f'    # sf bestmove2 cap promo:    {num_sf_bestmove2_captures:8d}')
            print(f'    # one good move:             {num_one_good_move:8d}')
            print(f'  # positions after filtering:   {num_positions_after_filter:8d}')
            print(f'    % positions kept:            {num_positions_after_filter/num_positions*100:8.1f}')


print(f'Processing {input_filename} ...')
if input_filename.endswith(".csv.zst"):
    with open(input_filename, 'rb') as compressed_infile, \
         open(output_filename, 'w+') as outfile:
        dctx = zstandard.ZstdDecompressor()
        stream_reader = dctx.stream_reader(compressed_infile)
        text_stream = io.TextIOWrapper(stream_reader, encoding='utf-8')
        process_csv_rows(text_stream, outfile)
else:
    with open(input_filename, 'r') as infile, \
         open(output_filename, 'w+') as outfile:
        process_csv_rows(infile, outfile)
num_positions_after_filter = num_positions - num_positions_filtered_out

print(f'Filtered {input_filename} to {output_filename}')
print(f'  # games:                       {num_games:8d}')
print(f'    # standard games:            {num_standard_games:8d}')
print(f'    # non-standard games:        {num_non_standard_games:8d}')
print(f'  # positions:                   {num_positions:8d}')
print(f'    # bestmove captures:         {num_bestmove_captures:8d}')
print(f'    # bestmove promos:           {num_bestmove_promos:8d}')
print(f'    # sf bestmove1 cap promo:    {num_sf_bestmove1_captures:8d}')
print(f'    # sf bestmove2 cap promo:    {num_sf_bestmove2_captures:8d}')
print(f'    # one good move:             {num_one_good_move:8d}')
print(f'  # positions after filtering:   {num_positions_after_filter:8d}')
print(f'    % positions kept:            {num_positions_after_filter/num_positions*100:8.1f}')
