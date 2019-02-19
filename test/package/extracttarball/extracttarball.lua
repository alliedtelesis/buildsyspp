require('buildsys')

bd = builddir()
bd:extract(bd:fetch { method = 'dl', uri = 'http://awpbuild.atlnz.lc/release_tarballs/dbus-1.2.16.tar.gz' })
