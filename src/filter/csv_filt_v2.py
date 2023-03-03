import io
import os.path
import sys

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
    output_filename = input_filename.replace('.csv.zst', '.csv.zst.filt-v3.plain')

if os.path.isfile(output_filename):
    print(f'Found .csv.plain file. Doing nothing: {output_filename}')
    sys.exit(0)

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

is_standard_game = False
num_standard_games = 0
num_non_standard_games = 0

def move_is_promo(uci_move):
    return len(uci_move) == 5 and uci_move[-1] in ['n','b','r','q']

def process_csv_rows(infile):
    global num_games, num_positions, num_positions_filtered_out, \
           num_bestmove_captures, num_bestmove_promos, \
           num_sf_bestmove1_captures, num_sf_bestmove2_captures, \
           num_standard_games, num_non_standard_games, \
           num_start_positions, num_early_plies, num_in_check, \
           num_one_good_move
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
        elif ply <= 28:
            # skip if an early ply position
            num_early_plies += 1
            should_filter_out = True
        elif b.is_check():
            # skip if in check
            num_in_check += 1
            should_filter_out = True
        else:
            b = chess.Board(fen)
            bestmove = chess.Move.from_uci(bestmove_uci)
            # filter out if provided move is a capture or promo
            bestmove_is_capture = b.is_capture(bestmove)
            bestmove_is_promo = move_is_promo(bestmove_uci)
            if bestmove_is_capture:
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
        if num_positions % 10000 == 0:
            num_positions_after_filter = num_positions - num_positions_filtered_out
            print(f'Processed {num_positions} positions')
            print(f'  # games:                       {num_games}')
            print(f'    # standard games:            {num_standard_games}')
            print(f'    # non-standard games:        {num_non_standard_games}')
            print(f'  # positions:                   {num_positions}')
            print(f'    # bestmove captures:         {num_bestmove_captures}')
            print(f'    # bestmove promos:           {num_bestmove_promos}')
            print(f'    # sf bestmove1 cap promo:    {num_sf_bestmove1_captures}')
            print(f'    # sf bestmove2 cap promo:    {num_sf_bestmove2_captures}')
            print(f'    # one good move:             {num_one_good_move}')
            print(f'  # positions after filtering:   {num_positions_after_filter}')
            print(f'    % positions kept:            {num_positions_after_filter/num_positions:.2f}')


print(f'Processing {input_filename} ...')
if input_filename.endswith(".csv.zst"):
    with open(input_filename, 'rb') as compressed_infile:
        dctx = zstandard.ZstdDecompressor()
        stream_reader = dctx.stream_reader(compressed_infile)
        text_stream = io.TextIOWrapper(stream_reader, encoding='utf-8')
        process_csv_rows(text_stream)
else:
    with open(input_filename, 'r') as infile: # , open(output_filename, 'w+') as outfile:
        process_csv_rows(infile)
num_positions_after_filter = num_positions - num_positions_filtered_out

print(f'Filtered {input_filename} to {output_filename}')
print(f'  # games:                       {num_games}')
print(f'    # standard games:            {num_standard_games}')
print(f'    # non-standard games:        {num_non_standard_games}')
print(f'  # positions:                   {num_positions}')
print(f'    # bestmove captures:         {num_bestmove_captures}')
print(f'    # bestmove promos:           {num_bestmove_promos}')
print(f'    # sf bestmove1 cap promo:    {num_sf_bestmove1_captures}')
print(f'    # sf bestmove2 cap promo:    {num_sf_bestmove2_captures}')
print(f'    # one good move:             {num_one_good_move}')
print(f'  # positions after filtering:   {num_positions_after_filter}')
print(f'    % positions kept:            {num_positions_after_filter/num_positions:.2f}')
