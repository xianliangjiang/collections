from util.helper import *

parser = argparse.ArgumentParser()
parser.add_argument('--file', '-f',
                    help="Data file",
                    required=True,
                    action="store",
                    dest="file")

parser.add_argument('--period', '-p',
                    help="Period",
                    required=True,
                    action="store",
                    dest="period")

parser.add_argument('-o',
                    help="output file",
                    required=True,
                    action="store",
                    dest="output")

args = parser.parse_args()

data = read_list(args.file)

count = 0
total = 0.0
largest = 0.0 

for row in data:
    try:
        ifname = row[1]
    except:
        break
    if ifname == 's1-eth2':
        num = float(row[2]) * 8.0 / (1 << 20)
        total = total + num
        count = count + 1
    
        if num > largest:
            largest = num 
        

avg = total / count
normalized = avg / 1.5

with open(args.output, "a") as out_file:
    out_file.write(str(args.period) + "," + str(normalized) + "\n")

