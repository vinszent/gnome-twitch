# Changelog

## v0.4.0
### Summary:
Mainly work on stability with much better error
handling and reporting, but also some key features have been
implemented.

* __Better moving and resizing of chat__

    When the chat is undocked it can now be easily moved and resized by
    selecting the `Move & resize chat` from the `Edit chat` submenu. The
    chat will then have a red border around it and can be moved and
    resized with the mouse instead of sliders.

* __Notifications have been re-enabled__

    Notifications are now enabled again. They can be clicked to start
    playing a channel.

* __Language selection__

    A language can now be chosen in the `Settings` menu. This will
    automatically filter channels according to the selected language
    where applicable.

* __Searching for offline channels__

    There is now a menu button that can be pressed to bring up search
    settings. From there one can select to also search for offline
    channels.

* __Display all stream qualities__

    Shows all available stream qualities including special ones like 720p60, etc.

* __Display all chat badges__

    Displays all chat badges including any news ones and temporary ones
    like the Horde/Alliance chat badges from the Warcraft event.

* __Dynamic loading of items in containers__

    No longer load a fixed number of items (channels, games, etc) but
    instead look at the allocation size at fetch an appropriate
    amount. This will speed up startup and refresh times.

* __Empty views__

    Implemented empty views that provide visual feedback instead of
    just a blank view.

* __Notification bar__

    Improved the notification bar so notifications can be queued and
    errors can also be displayed.

* __Viewer count for games__

    The viewer count for each game is now visible. Although the games
    are already sorted according to viewership, this helps gauging a
    game's popularity.

* __Changes to the build system__

    The Meson build scripts have been significantly cleaned up (with
    help from @TingPing).
