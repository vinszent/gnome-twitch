# Contributing
## Translating
### Zanata
You can edit translations with Red Hat's Zanata project using
GNOME Twitch's project
page
[here](https://translate.zanata.org/project/view/gnome-twitch). Zanata
provides a nice web interface to edit translations which makes it easier to contribute. There is a short startup guide to get you
going
[here](http://docs.zanata.org/en/release/user-guide/translator-guide/). If
you want a new language added, just create a new GitHub issue and I'll
enable it.
### Manually
1. Clone the latest source: `git clone https://github.com/vinszent/gnome-twitch`
2. Change to the clone directory: `cd gnome-twitch`
3. Setup Meson: `meson build`
4. Copy the template file to your chosen language (eg: `cp po/gnome-twitch.pot es.po`)
5. Edit the newly created template manually or with your favourite Gettext editor (gtranslator, poedit, etc)
6. Add your language to the langs array in `po/meson.build`
7. Add the language code to the LINGUAS file in the po directory (notice the file is sorted alphabetically)
8. Test your translation by running `ninja install && gnome-twitch` (needs root to install) from the `build` directory.

## Development
1. Clone the latest source: `git clone https://github.com/vinszent/gnome-twitch`
2. Change to the clone directory: `cd gnome-twitch`
3. Setup Meson: `meson build`
4. Create a new feature branch and switch to it: `git checkout -b myfeature`
5. Write code!
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
