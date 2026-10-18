#!/usr/bin/env bash
# Runs every topology x busy level and keeps each run's files in its own folder.
# Usage:  ./run_experiments.sh            (needs ./sim built from t1.c + task2basestation.c)
# Override with env vars, e.g.:  SIM=60 WIN=5 REPS=3 BUSY="50 95" ./run_experiments.sh
SIM=${SIM:-30}            # simulation seconds
WIN=${WIN:-3}             # cycle length = base station time window (seconds)
REPS=${REPS:-1}           # repeats per setting (each uses a different random seed)
BUSY=${BUSY:-"50 80 95 100"}            # % chance a port is busy; lower = more availability
GRIDS=${GRIDS:-"2x2 3x3 4x4 5x5"}       # m x n grids
LINES=${LINES:-"1x4 1x9 1x16 1x25"}     # line topologies with the same node counts
BIN=$(realpath ./sim) || exit 1
OUT=$(realpath -m ./results)
ROOT=""; [ "$(id -u)" = "0" ] && ROOT="--allow-run-as-root"

for topo in $GRIDS $LINES; do
  m=${topo%x*}; n=${topo#*x}; np=$((m * n + 1))      # nodes + 1 base station
  for b in $BUSY; do
    for r in $(seq 1 "$REPS"); do
      dir="$OUT/${topo}_busy${b}_rep${r}"
      mkdir -p "$dir"
      echo ">> $topo busy=$b% rep=$r  (np=$np)"
      ( cd "$dir" && timeout $((SIM * 3 + 60)) mpirun $ROOT --oversubscribe -np "$np" \
          "$BIN" "$m" "$n" "$SIM" "$WIN" "$b" "$r" > stdout.txt 2>&1 ) \
        || echo "   FAILED or timed out: $dir (see stdout.txt)"
    done
  done
done
echo "Done. Now run: python3 analyse.py --win $WIN"
