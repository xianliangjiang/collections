#!/bin/bash

echo "Building with args="  $@

INCLUDES="-I deps -I src -I . -I camio"
CFLAGS="-D_GNU_SOURCE -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -std=c11 -Werror -Wall -Wno-missing-field-initializers -Wno-unused-command-line-argument -Wno-missing-braces -Wno-array-bounds "
LINKFLAGS="-lexanic -lm "

SRC="dag_capture.c"
cake $SRC \
    --append-CFLAGS="$INCLUDES $CFLAGS" \
    --append-LINKFLAGS="$LINKFLAGS" \
    --no-git-root\
    --no-git-parent\
    $@



