depend('copyfile')

intercept()

bd = builddir()

bd:fetch('root','deps')

bd:cmd('root','cp',{'copy/file1',bd.new_install..'/file1'})
