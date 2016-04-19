require('buildsys')

bd = builddir()
bd:fetch('http://awpbuild.atlnz.lc/release_tarballs/dbus-1.2.16.tar.gz', 'dl')
bd:extract('dl/dbus-1.2.16.tar.gz')
