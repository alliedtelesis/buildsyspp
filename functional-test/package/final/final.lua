require('buildsys')
depend('interceptor')
depend('extracttarball')

bd = builddir()

bd:fetch {
    method = 'deps',
    to = 'test',
}

bd:shell('test','find -type f -exec sha256sum {} \\; > '..bd.new_install..'/output.txt')
