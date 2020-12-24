#!/bin/bash
set -e
SCRIPT_DIR=$(dirname $0)
if (( $# != 1 )); then
    printf "Usage: ./$0 <data file>\n"
    printf "Produces plot.pdf"
    exit 1
fi
gnuplot -e "filename='$1'" "$SCRIPT_DIR"/plot-evict-miss.gp 
