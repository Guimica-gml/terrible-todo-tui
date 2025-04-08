#!/usr/bin/env sh
set -xe

CFLAGS="-Wall -Wextra -pedantic -ggdb -std=c11"
CLIBS=""

mkdir -p build
gcc $CFLAGS -o build/todo-tui src/main.c src/utils.c $CLIBS
