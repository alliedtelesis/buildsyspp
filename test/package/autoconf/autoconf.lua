require('buildsys')
require('autotools')
require('fetch')

bd = autotools {
    fetch = fetch_url {
        package = 'gzip',
        url = 'http://ftp.gnu.org/pub/gnu/gzip/gzip-1.8.tar.gz',
    },
}
