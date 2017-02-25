#!/bin/sh

# Linux icons
for i in 16 22 24 32 48 256 512
do
    inkscape -z -e hicolor/$i\x$i/apps/com.vinszent.GnomeTwitch.png -w $i -h $i com.vinszent.GnomeTwitch.svg
done

# Windows icon
icotool -c \
        hicolor/16x16/apps/com.vinszent.GnomeTwitch.png \
        hicolor/32x32/apps/com.vinszent.GnomeTwitch.png \
        hicolor/48x48/apps/com.vinszent.GnomeTwitch.png \
        -o com.vinszent.GnomeTwitch.ico
