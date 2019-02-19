depend('copyfile','file1')

intercept()

bd = builddir()

bd:fetch {
    method = 'deps',
    to = 'root',
}

bd:cmd('root','cp',{'copy/file1',bd.new_install..'/file1'})
