#!/bin/bash

echo "Building with args="  $@
cake packet_gen.c $@

