sudo sysctl -w net.ipv4.ip_forward=1
./run.sh 2>results/out.txt
python process_log.py results/out.txt results/out.tsv
