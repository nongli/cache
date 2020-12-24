# Note you need gnuplot 4.4 for the pdfcairo terminal.

set terminal pdfcairo font "Gill Sans, 16" linewidth 6 rounded enhanced

# Line style for axes
set style line 80 lt rgb "#808080"

# Line style for grid
set style line 81 lt 0  # dashed
set style line 81 lt rgb "#808080"  # grey

set grid back linestyle 81
set border 3 back linestyle 80 # Remove border on top and right.  These
# borders are useless and make it harder to see plotted lines near the border.
# Also, put it in grey; no need for so much emphasis on a border.

set xtics nomirror
set ytics nomirror

set ylabel "Count"
set xlabel "Ghost Size"
set bars small
set datafile separator ","
set key outside bottom center horizontal
set output "plot.pdf"
plot filename using 2:3 w points lc 0  pt 1 title "Hits",\
     filename using 2:4 w points lc 1  pt 2 title "Misses" 
