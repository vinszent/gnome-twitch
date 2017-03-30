#!/bin/sh

#
# This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
# Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
#
# GNOME Twitch is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNOME Twitch is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
#

# This script assumes you are running it from the
# gnome-twitch/data/packages/windows directory

PACKAGE_DIR="gnome-twitch-chroot"
PACKAGE_VERSION=0.4
MINGW_ARCH="${MSYSTEM,,}"
LOG_NAME="build-log.txt"

touch "$LOG_NAME"

# Update versions
sed -i "s/%VERSION%/${PACKAGE_VERSION}/g" gnome-twitch.iss
sed -i "s/%MINGW_ARCH%/${MINGW_ARCH}/g" gnome-twitch.iss

# Create chroot environment
echo "==> Creating chroot environment"

mkdir "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}"/var/lib/pacman

pacman -Syu --noconfirm --root "${PACKAGE_DIR}" &>> "$LOG_NAME"

# Bootstrap chroot
echo "==> Bootstrapping chroot"
pacman -S --noconfirm --root "${PACKAGE_DIR}" bash filesystem pacman &>> "$LOG_NAME"

# Compile and install GT
echo "==> Compiling and installing packages"
makepkg -s -c --noconfirm &>> "$LOG_NAME"

pacman -U mingw-w64-"${MSYSTEM_CARCH}"-gnome-twitch-git*.tar.xz --root "${PACKAGE_DIR}" --noconfirm &>> "$LOG_NAME"

cd "${PACKAGE_DIR}/${MSYSTEM_PREFIX}"

# Remove unecessary files
echo "==> Removing unecessary files and optimizing"
find -name "*.a" | xargs rm -f # Remove all static libraries
find -not -name "gnome-twitch.exe" -name "*.exe" | xargs rm -f # Remove all executables as we probably won't need them, I hope
find bin -name "*" -not -name "*.exe" -not -name "*.dll" -type f | xargs rm -f # Remove leftover executables
rm -rf bin/libopencv* # Unusally large dlls that are not needed
rm -rf bin/gtk3-demo*.exe
rm -rf bin/gdbm*.exe
rm -rf bin/py*
rm -rf bin/*-config
rm -rf var/
rm -rf samples/
rm -rf libexec/
rm -rf sbin/
rm -rf include/
rm -rf share/man
rm -rf share/readline
rm -rf share/aclocal
rm -rf share/gnome-common
rm -rf share/glade
rm -rf share/gettext
rm -rf share/terminfo
rm -rf share/tabset
rm -rf share/pkgconfig
rm -rf share/bash-completion
rm -rf share/appdata
rm -rf share/gdb
rm -rf share/help
rm -rf share/gtk-doc
rm -rf share/doc
rm -rf share/vala
rm -rf share/cogl
rm -rf share/applications
rm -rf share/common-lisp
rm -rf share/OpenCV
rm -rf share/emacs
rm -rf share/libtool
rm -rf lib/terminfo
rm -rf lib/python2*
rm -rf lib/python3*
rm -rf lib/ruby
rm -rf lib/pkgconfig
rm -rf lib/peas-demo
rm -rf lib/cmake

# Strip all binaries of symbols for smaller size
find -name *.dll | xargs strip
find -name *.exe | xargs strip

# Remove unused translation files
find share/locale/ -type f | grep -v atk10.mo | grep -v libpeas.mo | grep -v gsettings-desktop-schemas.mo | grep -v json-glib-1.0.mo | grep -v glib20.mo | grep -v gdk-pixbuf.mo | grep -v gtk30.mo | grep -v gtk30-properties.mo | grep -v iso_*.mo | grep -v gnome-twitch.mo | grep -v gstreamer-1.0.mo | grep -v gst-plugins-*.mo | grep -v WebKitGTK-3.0.mo | xargs rm
find share/locale/ -type d | xargs rmdir -p --ignore-fail-on-non-empty

cd ../../

echo "==> Compiling installer"
C:/Program\ Files\ \(x86\)/Inno\ Setup\ 5/ISCC.exe gnome-twitch.iss &>> "$LOG_NAME"

# Cleanup
echo "==> Cleaning up"
rm -rf "$PACKAGE_DIR"
rm -f mingw-w64-"${MSYSTEM_CARCH}"-gnome-twitch-git*.tar.xz

echo ""
echo "Finished! Installer ready at gnome-twitch-$PACKAGE_VERSION-setup-32.exe"
echo "Build log available at build-log.txt"
