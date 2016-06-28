rm -f output_raw.txt
rm -f output_formatted.txt
rm -f improvement.txt

i=1
resp_size_list="900 5000 5000 9000 9500 9800 10000 11000 15000 30000"
for resp_size in $resp_size_list
do
    echo "Cleaning up Mininet topology..."
    sudo mn -c 2> /dev/null
    echo "Experiment $i of 10"
    python tcpcwnd.py -n 31 -r $resp_size -o output_raw.txt
    python formatOutput.py -f output_raw.txt -o output_formatted.txt
    python calculate_improvement.py -f output_formatted.txt -o improvement.txt
    i=$((i+1))
done

python average_and_plot.py -f improvement.txt -o results.png
