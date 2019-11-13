# Set output file
#set term postscript
set terminal png size 1000,800
set output "Random_Write_Latency.png"

# Set parametric
set parametric

# Set scale
#set autoscale
set logscale x

# Set x y
set format x "10^{%L}"
set xtic 10
set xrange [10:]

set format y "%.0f%%"
set ytic 25

set xlab "Latency(ns)"
set ylab "Write Completed in Time(%)"

# style line

set style line 1 lt 1 lw 3 lc 1
set style line 2 lt 1 lw 3 lc 2
set style line 3 lt 1 lw 3 lc 3
set style line 4 lt 1 lw 3 lc 4
set style line 5 lt 1 lw 3 lc 5
set style line 6 lt 1 lw 3 lc 6
set style line 7 lt 1 lw 3 lc 7

plot "./plot_data/1024B_write.dat" u 1:2 with l ls 1 title "1KB", \
     "./plot_data/4096B_write.dat" u 1:2 with l ls 2 title "4KB", \
     "./plot_data/16384B_write.dat" u 1:2 with l ls 3 title "16KB"
#     "./plot_data/4096B_write.dat" u 1:2 with l ls 4 title "4KB", \
#     "./plot_data/16384B_write.dat" u 1:2 with l ls 5 title "16KB", \
#     "./plot_data/65536B_write.dat" u 1:2 with l ls 6 title "64KB", \
#     "./plot_data/262144B_write.dat" u 1:2 with l ls 7 title "256KB"
