set (LOCALE_DIR "/usr/share/locale/")

configure_file (
  "${PROJECT_SOURCE_DIR}/src/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )
configure_file (
  "${PROJECT_SOURCE_DIR}/src/version.h.in"
  "${PROJECT_BINARY_DIR}/version.h"
  )
include_directories("${PROJECT_BINARY_DIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

pkg_check_modules(GST REQUIRED gstreamer-video-1.0)
include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})
add_definitions(${GST_CFLAGS_OTHER})

pkg_check_modules(SOUP REQUIRED libsoup-2.4)
include_directories(${SOUP_INCLUDE_DIRS})
link_directories(${SOUP_LIBRARY_DIRS})
add_definitions(${SOUP_CFLAGS_OTHER})

pkg_check_modules(JSON REQUIRED json-glib-1.0)
include_directories(${JSON_INCLUDE_DIRS})
link_directories(${JSON_LIBRARY_DIRS})
add_definitions(${JSON_CFLAGS_OTHER})

add_definitions("-std=c11")

SET( SRCS main.c gt-app.c gt-win.c gt-twitch.c gt-channel.c gt-player.c gt-channels-view.c gt-channels-view-child.c 
 gt-player-header-bar.c gt-browse-header-bar.c gt-game.c gt-games-view.c gt-games-view-child.c gt-settings-dlg.c 
 gt-favourites-manager.c gt-enums.c utils.c ../data/gnome-twitch.c)
add_executable(gnome-twitch ${SRCS})
target_link_libraries(gnome-twitch ${GST_LIBRARIES})
target_link_libraries(gnome-twitch ${SOUP_LIBRARIES})
target_link_libraries(gnome-twitch ${JSON_LIBRARIES})
target_link_libraries(gnome-twitch ${GTK3_LIBRARIES})

install (TARGETS gnome-twitch DESTINATION bin)
