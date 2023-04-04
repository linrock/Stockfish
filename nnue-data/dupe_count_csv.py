from glob import glob
import io
import os.path
import sys

import chess
import zstandard

''' Iterate over positions .csv files and output .plain files
'''
if len(sys.argv) != 2:
    print('Usage: ./dupe_count_csv.py <input_csv_file>')
    sys.exit(0)

input_filename_glob = sys.argv[1]

position = None
num_games = 0
num_positions = 0
num_positions_filtered_out = 0
num_bestmove_captures = 0
num_bestmove_promos = 0
num_sf_bestmove1_captures = 0
num_sf_bestmove2_captures = 0
num_overlap_sf_bestmove_captures = 0

is_standard_game = False
num_standard_games = 0
num_non_standard_games = 0
num_unique_piece_orientations = 0

num_ply_gt_30 = 0
num_unique_gt_30 = 0

num_ply_gt_28 = 0
num_unique_gt_28 = 0

num_ply_gt_24 = 0
num_unique_gt_24 = 0

num_ply_gt_20 = 0
num_unique_gt_20 = 0

num_ply_lteq_20 = 0
num_unique_lteq_20 = 0

piece_orientations_seen = set()

def move_is_promo(uci_move):
    return len(uci_move) == 5 and uci_move[-1] in ['n','b','r','q']

def process_csv_rows(infile):
    global num_games, num_positions, num_positions_filtered_out, \
           num_bestmove_captures, num_bestmove_promos, num_sf_bestmove1_captures, num_sf_bestmove2_captures, \
           num_standard_games, num_non_standard_games, num_overlap_sf_bestmove_captures, num_unique_piece_orientations, \
           num_ply_gt_30, num_unique_gt_30, \
           num_ply_gt_28, num_unique_gt_28, \
           num_ply_gt_24, num_unique_gt_24, \
           num_ply_gt_20, num_unique_gt_20, \
           num_ply_lteq_20, num_unique_lteq_20
    for row in infile:
        split_row = row.strip().split(",")
        if len(split_row) == 10:
            ply, fen, bestmove_uci, bestmove_score, game_result, \
            sf_search_method, sf_bestmove1_uci, sf_bestmove1_score, sf_bestmove2_uci, sf_bestmove2_score = \
                split_row
        elif len(split_row) == 8:
            ply, fen, bestmove_uci, bestmove_score, game_result, \
            sf_search_method, sf_bestmove1_uci, sf_bestmove1_score = split_row
            sf_bestmove2_uci, sf_bestmove2_score = None, None
        ply = int(ply)
        if ply == 0:
            num_games += 1
            if 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq' in fen:
                num_standard_games += 1
            else:
                num_non_standard_games += 1
        piece_orientation = fen.split(' ')[0]
        if ply > 30:
            if piece_orientation not in piece_orientations_seen:
                num_unique_gt_30 += 1
            num_ply_gt_30 += 1
        if ply > 28:
            if piece_orientation not in piece_orientations_seen:
                num_unique_gt_28 += 1
            num_ply_gt_28 += 1
        if ply > 24:
            if piece_orientation not in piece_orientations_seen:
                num_unique_gt_24 += 1
            num_ply_gt_24 += 1
        if ply > 20:
            if piece_orientation not in piece_orientations_seen:
                num_unique_gt_20 += 1
            num_ply_gt_20 += 1
        if ply <= 20:
            if piece_orientation not in piece_orientations_seen:
                num_unique_lteq_20 += 1
            num_ply_lteq_20 += 1
        if piece_orientation not in piece_orientations_seen:
            num_unique_piece_orientations += 1
        piece_orientations_seen.add(piece_orientation)
        num_positions += 1
        if (num_positions % 1000000 == 0) and num_positions > 0:
            print(f"Processed {num_positions} positions")
            print(f'  # standard games:           {num_standard_games}')
            print(f'  # non-standard games:       {num_non_standard_games}')
            print(f'  # positions:                {num_positions}')
            print(f'    # unique:                 {num_unique_piece_orientations}')
            print(f'    % unique:                 {num_unique_piece_orientations / num_positions * 100:.2f}')
            print(f'  # positions ply > 30:       {num_ply_gt_30}')
            print(f'    # unique:                 {num_unique_gt_30}')
            print(f'    % unique:                 {num_unique_gt_30 / num_ply_gt_30 * 100:.2f}')
            print(f'  # positions ply > 28:       {num_ply_gt_28}')
            print(f'    # unique:                 {num_unique_gt_28}')
            print(f'    % unique:                 {num_unique_gt_28 / num_ply_gt_28 * 100:.2f}')
            print(f'  # positions ply > 24:       {num_ply_gt_24}')
            print(f'    # unique:                 {num_unique_gt_24}')
            print(f'    % unique:                 {num_unique_gt_24 / num_ply_gt_24 * 100:.2f}')
            print(f'  # positions ply > 20:       {num_ply_gt_20}')
            print(f'    # unique:                 {num_unique_gt_20}')
            print(f'    % unique:                 {num_unique_gt_20 / num_ply_gt_20 * 100:.2f}')
            print(f'  # positions ply <= 20:      {num_ply_lteq_20}')
            print(f'    # unique:                 {num_unique_lteq_20}')
            print(f'    % unique:                 {num_unique_lteq_20 / num_ply_lteq_20 * 100:.2f}')

for input_filename in glob(input_filename_glob):
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

print(f"Processed {num_positions} positions")
print(f'  # standard games:           {num_standard_games}')
print(f'  # non-standard games:       {num_non_standard_games}')
print(f'  # positions:                {num_positions}')
print(f'    # unique:                 {num_unique_piece_orientations}')
print(f'    % unique:                 {num_unique_piece_orientations / num_positions * 100:.2f}')
print(f'  # positions ply > 30:       {num_ply_gt_30}')
print(f'    # unique:                 {num_unique_gt_30}')
print(f'    % unique:                 {num_unique_gt_30 / num_ply_gt_30 * 100:.2f}')
print(f'  # positions ply > 28:       {num_ply_gt_28}')
print(f'    # unique:                 {num_unique_gt_28}')
print(f'    % unique:                 {num_unique_gt_28 / num_ply_gt_28 * 100:.2f}')
print(f'  # positions ply > 24:       {num_ply_gt_24}')
print(f'    # unique:                 {num_unique_gt_24}')
print(f'    % unique:                 {num_unique_gt_24 / num_ply_gt_24 * 100:.2f}')
print(f'  # positions ply > 20:       {num_ply_gt_20}')
print(f'    # unique:                 {num_unique_gt_20}')
print(f'    % unique:                 {num_unique_gt_20 / num_ply_gt_20 * 100:.2f}')
print(f'  # positions ply <= 20:      {num_ply_lteq_20}')
print(f'    # unique:                 {num_unique_lteq_20}')
print(f'    % unique:                 {num_unique_lteq_20 / num_ply_lteq_20 * 100:.2f}')
