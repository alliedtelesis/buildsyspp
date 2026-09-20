require('buildsys')
depend('interceptor')
depend('extracttarball')
depend('copydir')

bd = builddir()

bd:fetch {
    method = 'deps',
    to = 'test',
}

bd:shell('test','find -type f -exec sha256sum {} \\; | sort > '..bd.new_install..'/output.txt')
