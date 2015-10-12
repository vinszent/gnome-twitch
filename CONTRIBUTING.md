# Contributing
## Translating
1. Clone the latest source: `git clone https://github.com/Ippytraxx/gnome-twitch`
2. Change to the clone directory: `cd gnome-twitch`
3. Setup Meson: `mkdir build && meson . build`
4. Create a new Gettext template: `cd build && ninja pot`
5. Edit the newly created template in the `po` directory (`gnome-twitch/po/gnome-twitch.po`) either manually or with your favourite Gettext editor (gtranslator, poedit, etc)
6. Rename your translated template to `language_code.po`, ex: Swedish = sv.po, German = de.po, Chinese = zh.po 
7. Add your language to the langs array in `po/meson.build`
8. Test your translation by running `ninja install && gnome-twitch` (needs root to install) from the `build` directory.

## Development
Soon

