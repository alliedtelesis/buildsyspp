require('buildsys')
bd = builddir()

bd:fetch {
    method = 'copyfile',
    uri = 'file1',
}

bd:mkdir(bd.new_install,{'copy'})
bd:cmd('','cp',{'file1',bd.new_install..'/copy/'..name()})
