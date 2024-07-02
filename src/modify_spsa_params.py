import sys


if len(sys.argv) != 2:
    print("Usage: python3 modify_spsa_params.py <filename>")
    sys.exit(0)

# spsa params from ./stockfish output
filename = sys.argv[1]
output_filename = filename.replace(".txt", ".fixed-c.txt")

print(f"Modifying spsa params in: {filename}")
with open(filename, "r") as f:
    rows = [row.split(",") for row in f.read().strip().split("\n")]

# c value 4 for weights
# c value 128 for biases
print(f"Output params in: {output_filename}")
with open(output_filename, "w") as f:
    for row in rows:
        if row[0].split("[")[0][-1] == "W":
            row[4] = "4"
        elif row[0].split("[")[0][-1] == "B":
            row[4] = "128"
        f.write(",".join(row) + "\n")
