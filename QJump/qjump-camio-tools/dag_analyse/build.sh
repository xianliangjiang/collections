#!/bin/bash

echo "Building with args="  $@
cake dag_analyse.c $@ --append-CFLAGS="-D_GNU_SOURCE -std=c11" --append-LINKFLAGS="-lcrypto -lssl -lexanic"

