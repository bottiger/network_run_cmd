CMD=/usr/bin/gcc `/usr/bin/pkg-config --cflags dbus-1` `/usr/bin/pkg-config --cflags glib-2.0` `/usr/bin/pkg-config --cflags dbus-glib-1` signals-tutorial.c `/usr/bin/pkg-config --libs dbus-1` `/usr/bin/pkg-config --libs glib-2.0` `/usr/bin/pkg-config --libs dbus-glib-1`

main: $(CMD)
