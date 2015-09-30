#!/bin/bash

if [ -d .git ]; then
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
else
    exit 1
fi;
