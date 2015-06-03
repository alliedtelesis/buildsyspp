depend('interceptor')

bd = builddir()

bd:fetch('test','deps')

bd:shell('test','find -type f -exec sha256sum {} \\; > '..bd.new_install..'/output.txt')
