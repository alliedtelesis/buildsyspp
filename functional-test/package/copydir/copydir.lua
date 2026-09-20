require('buildsys')
bd = builddir()

-- Copy a small directory into 'dest' first (using an explicit 'to', rather
-- than the default '.'), so the directory copy below has to merge into an
-- already-populated destination rather than create it from scratch.
bd:fetch {
    method = 'copyfile',
    uri = 'existing',
    to = 'dest',
}

-- Recursively copy a directory tree into the same destination. Its contents
-- should land alongside 'existing.txt', preserving the tree's own relative
-- layout, not nested under an extra 'tree/' directory.
bd:fetch {
    method = 'copyfile',
    uri = 'tree',
    to = 'dest',
}

bd:mkdir(bd.new_install,{'copy'})
bd:cmd('','cp',{'-r','dest',bd.new_install..'/copy/'..name()})
