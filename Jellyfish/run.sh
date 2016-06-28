#!/bin/bash
sudo mn -c

for i in 0.25 0.5 0.75 1.0; do
    python jellyfish.py -t jf -r ksp -dir link_results -nse 16 -nsw 20 -np 4 -e l -p $i
    python jellyfish.py -t jf -r ecmp -dir link_results -nse 16 -nsw 20 -np 4 -e l -p $i

    python plot_links.py -dir link_results -o result-$i.png -p $i
done

for f in 1 2 8; do
    for i in 1 2 3; do
	python jellyfish.py -t jf -r ksp -dir tp_results_jf_ksp_$i -nse 16 -nsw 20 -np 4 -e t -f $f
	python jellyfish.py -t jf -r ecmp -dir tp_results_jf_ecmp_$i -nse 16 -nsw 20 -np 4 -e t -f $f
	python jellyfish.py -t ft -r hashed -np 4 -dir tp_results_ft_ecmp_$i -e t -f $f
    done

    python compile_throughput.py -dir tp_results -o result-throughput$f.txt -f $f
    rm -r tp_results* # so second run does not double count files
done
