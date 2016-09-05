#!/bin/bash

echo "Building with args="  $@
cake camio_perf_s.c $@
cake camio_perf_c.c $@

