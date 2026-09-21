require('buildsys')
bd = builddir()

-- bd:exists() must tell a real (nested-only) directory apart from one that
-- genuinely isn't there -- unlike bd:ls(), which returns an empty list for
-- both, since it discards directory entries from its listing.
assert(bd:exists('existing') == true, 'bd:exists() should find existing/')
assert(bd:exists('tree') == true, 'bd:exists() should find tree/')
assert(bd:exists('this-does-not-exist-at-all') == false,
    'bd:exists() should not find a nonexistent directory')

-- relative_fetch_path() trusts absolute (and 'dl/') locations without
-- checking they exist, since most of its callers just want the path
-- resolved, not verified. bd:exists() must still check, or every
-- nonexistent absolute path would look like it exists.
assert(bd:exists('/this-does-not-exist-at-all-either') == false,
    'bd:exists() should not find a nonexistent absolute path')

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
