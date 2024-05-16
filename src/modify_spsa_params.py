with open("params.txt", "r") as f:
    rows = [row.split(",") for row in f.read().strip().split("\n")]

with open("params-fixed-c.txt", "w") as f:
    for row in rows:
        if row[0].split("[")[0][-1] == "w":
            row[4] = "4"
        elif row[0].split("[")[0][-1] == "b":
            row[4] = "128"
        f.write(",".join(row) + "\n")
