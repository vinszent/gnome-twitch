#!/bin/bash

for i in 16 22 24 32 48 256 512
do
    inkscape -z -e hicolor/$i\x$i/apps/com.vinszent.GnomeTwitch.png -w $i -h $i com.vinszent.GnomeTwitch.svg
done
