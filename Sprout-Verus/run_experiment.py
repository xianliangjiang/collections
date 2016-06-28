#! /usr/bin/python
import os

if __name__ == "__main__":

	RUN_VERUS_ALMA_SHORT = "scripts/run-verus-red-alma-short" # verus_alma_short_log
	RUN_VERUS_VAR = "scripts/run-verus-red-variable" # verus_var_log
        
	RUN_SPROUT_ALMA_SHORT = "scripts/run-sprout-red-alma-short" # sprout_alma_short_log
	RUN_SPROUT_VAR = "scripts/run-sprout-red-variable" # sprout_var_log

	print("Welcome to Ryandrew's experiment.")
	print("This will take a LONG time to run.")
	print("Feel free to go do whatever for the next 35 minutes or so.")
	os.system("mkdir logs")
	os.system("mkdir results")

	os.system(RUN_VERUS_ALMA_SHORT)
	os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_alma_short_log > ./results/verus_alma_throughput.html") 
	os.system("mm-delay-graph ./logs/verus_alma_short_log > ./results/verus_alma_delay.html")
	print("Finished running the first test!")

	os.system(RUN_VERUS_VAR)
	os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_var_log > ./results/verus_var_throughput.html") 
	os.system("mm-delay-graph ./logs/verus_var_log > ./results/verus_var_delay.html")
	print("Finished running the second test!")	

	os.system(RUN_SPROUT_ALMA_SHORT)
	os.system("mm-throughput-graph 500 ./logs/sprout_alma_short_log > ./results/sprout_alma_throughput.html") 
	os.system("mm-delay-graph ./logs/sprout_alma_short_log > ./results/sprout_alma_delay.html")
	print("Finished running the third test!")

	os.system(RUN_SPROUT_VAR)
	os.system("mm-throughput-graph 500 ./logs/sprout_var_log > ./results/sprout_var_throughput.html") 
	os.system("mm-delay-graph ./logs/sprout_var_log > ./results/sprout_var_delay.html")
	print("Finished running the fourth test!")

        os.system("./plot_scripts/mm-throughput-comp-graph 500 ./logs/sprout_var_log ./logs/verus_var_log > ./results/var_throughput_comp.html")

	print("Finished! Look in the results directory to find the html files to download and view.")
