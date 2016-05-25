# Contributing
## Translating
1. Clone the latest source: `git clone https://github.com/Ippytraxx/gnome-twitch`
2. Change to the clone directory: `cd gnome-twitch`
3. Setup Meson: `mkdir build && meson . build`
4. Create a new Gettext template: `cd build && ninja pot`
5. Edit the newly created template in the `po` directory (`gnome-twitch/po/gnome-twitch.po`) either manually or with your favourite Gettext editor (gtranslator, poedit, etc)
6. Rename your translated template to `language_code.po`, ex: Swedish = sv.po, German = de.po, Chinese (traditional) = zh_TW.po
7. Add your language to the langs array in `po/meson.build`
8. Test your translation by running `ninja install && gnome-twitch` (needs root to install) from the `build` directory.

## Development
1. Clone the latest source: `git clone https://github.com/Ippytraxx/gnome-twitch`
2. Change to the clone directory: `cd gnome-twitch`
3. Setup Meson: `mkdir build && meson . build`
4. Create a new feature branch and switch to it: `git checkout -b myfeature`
5. Write code. Feel free to drop the Gitter channel for help! [![Gitter](https://badges.gitter.im/Ippytraxx/gnome-twitch.svg)](https://gitter.im/Ippytraxx/gnome-twitch?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
6. Test your code: `cd build && ninja && ./src/gnome-twitch`
7. Add your changes: `git add -A`
8. Commit your changes: `git commit -m 'Short message'`
9. Push changes to your GitHub repo.
10. Submit a PR request!

### Coding style
I use Allman/BSD style indenting; I'm usually not too picky but there are a few key points:

* Braces always on a new line, for functions and control statements.

* Space between control statement and parenthesis but not for functions, i.e.:

```
while (TRUE)
{
    ...
}
```

_(See next point for function example)_

* Function declarations are split, i.e.:

```
static void
my_func(gpointer data)
{
    ...
}
```

* Pointer stars are put after the type, not before the variable, i.e.:

```
gchar* my_str;
```

* Not a syntax thing but use GLib types whenever possible, i.e.:

`gchar` instead of `char`, `gint` instead of `int`, etc.
