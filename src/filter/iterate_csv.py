import os.path
import sys

import chess

''' Iterate over positions .csv files and output .plain files
'''
if len(sys.argv) != 2:
    print('Usage: ./iterate_csv.py <input_csv_file>')
    sys.exit(0)

input_filename = sys.argv[1]
output_filename = input_filename.replace('.csv', '.csv.plain')

if os.path.isfile(output_filename):
    print(f'Found .csv.plain file. Doing nothing: {output_filename}')
    sys.exit(0)

position = None
num_games = 0
num_positions = 0
num_bestmove_captures = 0
num_bestmove_promos = 0

is_standard_game = False
num_standard_games = 0
num_non_standard_games = 0

print(f'Processing {input_filename} ...')
with open(input_filename, 'r') as infile: # , open(output_filename, 'w+') as outfile:
    for row in infile:
        ply, fen, bestmove, bestmove_score, game_result, sf_search_method, bestmove1, bestmove1_score, bestmove2, bestmove2_score = \
            row.strip().split(",")
        ply = int(ply)
        if ply == 0:
            num_games += 1
            if 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq' in fen:
                num_standard_games += 1
            else:
                num_non_standard_games += 1
        b = chess.Board(fen)
        move = chess.Move.from_uci(bestmove)
        bestmove_is_capture = b.is_capture(move)
        if bestmove_is_capture:
            num_bestmove_captures += 1
        bestmove_is_promo = len(bestmove) == 5 and bestmove[-1] in ['n','b','r','q']
        if bestmove_is_promo:
            num_bestmove_promos += 1
        num_positions += 1

print(f'Filtered {input_filename} to {output_filename}')
print(f'  # games:                 {num_games}')
print(f'    # standard games:      {num_standard_games}')
print(f'    # non-standard games:  {num_non_standard_games}')
print(f'  # positions:             {num_positions}')
print(f'    # bestmove captures:   {num_bestmove_captures}')
print(f'    # bestmove promos:     {num_bestmove_promos}')
