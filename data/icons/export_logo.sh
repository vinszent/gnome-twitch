#!/bin/bash

for i in 16 22 24 32 48 256 512
do
    inkscape -z -e $i\x$i/hicolor/apps/gnome-twitch.png -w $i -h $i gnome-twitch.svg
done
