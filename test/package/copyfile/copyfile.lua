bd = builddir()

bd:fetch('file1','copyfile')

bd:mkdir(bd.new_install,{'copy'})
bd:cmd('','cp',{'file1',bd.new_install..'/copy'})
