#! /usr/bin/python
import os

if __name__ == "__main__":

        RUN_VERUS_ALMA_LONG = "scripts/run-verus-red-alma-long" # verus_alma_long_log
	RUN_VERUS_ALMA_SHORT = "scripts/run-verus-red-alma-short" # verus_alma_short_log
        RUN_VERUS_VERIZON = "scripts/run-verus-red-verizon" # verus_verizon_log
        RUN_VERUS_ATT = "scripts/run-verus-red-att" # verus_att_log
	RUN_VERUS_VAR = "scripts/run-verus-red-variable" # verus_var_log
        
        RUN_SPROUT_ALMA_LONG = "scripts/run-sprout-red-alma-long" # sprout_alma_long_log
	RUN_SPROUT_ALMA_SHORT = "scripts/run-sprout-red-alma-short" # sprout_alma_short_log
        RUN_SPROUT_VERIZON = "scripts/run-sprout-red-verizon" # sprout_verizon_log
        RUN_SPROUT_ATT = "scripts/run-sprout-red-att" # sprout_att_log
	RUN_SPROUT_VAR = "scripts/run-sprout-red-variable" # sprout_var_log

	print("Welcome to Ryandrew's experiment.")
	print("This will take a LONG time to run.")
	print("Feel free to go do whatever for the next 2 hours or so.")
	os.system("mkdir logs")
	os.system("mkdir results")

	os.system(RUN_VERUS_ALMA_SHORT)
	os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_alma_short_log > ./results/verus_alma_throughput.html") 
	os.system("mm-delay-graph ./logs/verus_alma_short_log > ./results/verus_alma_delay.html")
	print("Finished running the first test!")

        os.system(RUN_VERUS_ALMA_LONG)
        os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_alma_long_log > ./results/verus_alma_long_throughput.html")
        os.system("mm-delay-graph ./logs/verus_alma_long_log > ./results/verus_alma_long_delay.html")
        print("Finished running the second test!")

        os.system(RUN_VERUS_VERIZON)
        os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_verizon_log > ./results/verus_verizon_throughput.html")
        os.system("mm-delay-graph ./logs/verus_verizon_log > ./results/verus_verizon_delay.html")
        print("Finished running the third test!")
        
        os.system(RUN_VERUS_ATT)
        os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_att_log > ./results/verus_att_throughput.html")
        os.system("mm-delay-graph ./logs/verus_att_log > ./results/verus_att_delay.html")
        print("Finished running the fourth test!")

	os.system(RUN_VERUS_VAR)
	os.system("./plot_scripts/mm-throughput-graph-verus 500 ./logs/verus_var_log > ./results/verus_var_throughput.html") 
	os.system("mm-delay-graph ./logs/verus_var_log > ./results/verus_var_delay.html")
	print("Finished running the fifth test!")	

	os.system(RUN_SPROUT_ALMA_SHORT)
	os.system("mm-throughput-graph 500 ./logs/sprout_alma_short_log > ./results/sprout_alma_throughput.html") 
	os.system("mm-delay-graph ./logs/sprout_alma_short_log > ./results/sprout_alma_delay.html")
	print("Finished running the sixth test!")

        os.system(RUN_SPROUT_ALMA_LONG)
        os.system("mm-throughput-graph 500 ./logs/sprout_alma_long_log > ./results/sprout_alma_long_throughput.html")
        os.system("mm-delay-graph ./logs/sprout_alma_long_log > ./results/sprout_alma_long_delay.html")
        print("Finished running the seventh test!")

        os.system(RUN_SPROUT_VERIZON)
        os.system("mm-throughput-graph 500 ./logs/sprout_verizon_log > ./results/sprout_verizon_throughput.html")
        os.system("mm-delay-graph ./logs/sprout_verizon_log > ./results/sprout_verizon_delay.html")
        print("Finished running the eighth test!")

        os.system(RUN_SPROUT_ATT)
        os.system("mm-throughput-graph 500 ./logs/sprout_att_log > ./results/sprout_att_throughput.html")
        os.system("mm-delay-graph ./logs/sprout_att_log > ./results/sprout_att_delay.html")
        print("Finished running the ninth test!")
        
	os.system(RUN_SPROUT_VAR)
	os.system("mm-throughput-graph 500 ./logs/sprout_var_log > ./results/sprout_var_throughput.html") 
	os.system("mm-delay-graph ./logs/sprout_var_log > ./results/sprout_var_delay.html")
	print("Finished running the tenth test!")

        os.system("./plot_scripts/mm-throughput-comp-graph 500 ./logs/sprout_var_log ./logs/verus_var_log > ./results/var_throughput_comp.html")

	print("Finished! Look in the results directory to find the html files to download and view.")
