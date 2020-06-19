require('buildsys')
require('autotools')
require('fetch')

bd = autotools {
    fetch = fetch_url {
        package = 'gzip',
        url = 'https://ftp.gnu.org/pub/gnu/gzip/gzip-1.10.tar.gz',
    },
}
