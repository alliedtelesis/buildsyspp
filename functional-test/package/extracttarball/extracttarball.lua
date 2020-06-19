require('buildsys')

bd = builddir()
bd:extract(
    bd:fetch {
        method = 'dl',
        uri = 'https://dbus.freedesktop.org/releases/dbus/dbus-1.2.16.tar.gz',
    }
)
