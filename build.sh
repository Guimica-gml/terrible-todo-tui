#!/usr/bin/env sh
set -xe

CFLAGS="-Wall -Wextra -pedantic -ggdb -std=c11"
CLIBS=""

gcc $CFLAGS -o todo-tui src/main.c $CLIBS
