#!/bin/sh

if test "$(git rev-parse --is-inside-work-tree 2> /dev/null)" = "true"; then
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
else
    exit 1
fi;
