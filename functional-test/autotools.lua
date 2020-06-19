require('buildsys')

--[[
Use this helper when building an 'autotools' package
It will run:
- configure
- make
- make install

Pass an array of the controlling arguments as the only variable.
I.e.
bd = autotools {
    arg_name = 'value',
    multi_arg_name = {'value1', 'value2'},
}

Supported arguments:
- fetch, the fetch object to use
- build_dir, the directory to build the package in
    (if not present, the package is built in the source directory)
- autoreconf, whether to run autoreconf or not (default: not fetch:dist())
- autoreconf_args, extra arguments to autoreconf
- autoreconf_env, extra environment variabes when running autoreconf
- autoreconf_cmd, autoreconf command to run (default 'autoreconf')
- cflags_remove, pattern to match to remove from cflags
    (i.e. '-W[^%s]*' to remove all -Werror -Wall, etc)
- cppflags_extra, additional string to add to cppflags
- cflags_extra, additional string to add to cflags
- ldflags_extra, additional string to add to ldflags
- libs_extra, additional string to add to libs
- libs_extra_dirs, extra -L directories to add to libs
- includes_extra_dirs, extra -I directories to add to cppflags
- config_args, extra arguments to ./configure
- cache_configure, enable ./configure caching (default: yes)
- shared_libs, enable shared libraries to be built (default: yes)
- static_libs, enable static libraries to be built (default: no)
- prefix, --prefix=VALUE to use, defaults to '/usr'
- config_env, extra environment variables when running ./configure
- make_job_limit, job limit to use
    (if not present, defaults to feature('job-limit'))
- make_args, extra arguments to make
- install_install, set to no to disable "make install DESTDIR=new_install"
    (by default, make install will be done with DESTDIR=new_install)
- install_staging, set to yes to enable "make install DESTDIR=new_staging"
    (by default, this will not occur)
- install_env, extra environment variables when running "make install..."
]]

function autotools(args)
    local obj = builddir()

    local fetch = args.fetch
    local sourcedir = fetch:sourcedir()

    -- directory to build package in
    local builddir = sourcedir
    local rel_sourcedir = './'
    if (args.build_dir) then
        obj:mkdir('',{args.build_dir})
        builddir = args.build_dir
        rel_sourcedir = '../' .. sourcedir .. '/'
    end

    -- autoreconf
    local autoreconf = not fetch:dist()
    if (args.autoreconf ~= nil) then
        autoreconf = args.autoreconf
    end
    if (autoreconf and autoreconf ~= 'no') then
        local autoreconf_cmd = 'autoreconf'
        local autoreconf_args = {}
        local autoreconf_env = {}

        if (args.autoreconf_cmd ~= nil) then
            autoreconf_cmd = args.autoreconf_cmd
        end

        if type(args.autoreconf_args) == "table" then
            for _,v in pairs(args.autoreconf_args) do
                table.insert(autoreconf_args, v)
            end
        else
            table.insert(autoreconf_args, args.autoreconf_args)
        end

       if type(args.autoreconf_env) == "table" then
            for _,v in pairs(args.autoreconf_env) do
                table.insert(autoreconf_env, v)
            end
        else
            table.insert(autoreconf_env, args.autoreconf_env)
        end

        obj:cmd(sourcedir, autoreconf_cmd, autoreconf_args, autoreconf_env)
    end

    local prefix = '/usr'
    if (args.prefix ~= nil) then
        prefix = args.prefix
    end

    -- configure
    local conf_opts = {}
    local cflags = ''
    local ldflags = ''
    local cppflags = ''
    local libs = ''
    if feature('target-tuple') then
        table.insert (conf_opts, '--target=' .. feature('target-tuple'))
        table.insert (conf_opts, '--host=' .. feature('target-tuple'))
        cflags = cflags .. ' --sysroot=' .. obj.staging
        ldflags = ldflags .. ' --sysroot=' .. obj.staging
    end
    table.insert (conf_opts, '--prefix=' .. prefix)
    if feature('cflags') then
        cflags = cflags .. ' ' .. feature('cflags')
    end
    if (args.cflags_remove) then
        cflags = string.gsub(cflags, args.cflags_remove, '')
    end
    if (args.cppflags_extra) then
        cppflags = cppflags .. ' ' .. args.cppflags_extra
    end
    if (args.cflags_extra) then
        cflags = cflags .. ' ' .. args.cflags_extra
    end
    if (args.ldflags_extra) then
        ldflags = ldflags .. ' ' .. args.ldflags_extra
    end
    if (args.libs_extra) then
        libs = libs .. ' ' .. args.libs_extra
    end
    if (args.libs_extra_dirs) then
        for _,v in pairs(args.libs_extra_dirs) do
            ldflags = ldflags .. ' -L' .. obj.staging.. v
        end
    end
    if (args.includes_extra_dirs) then
        for _,v in pairs(args.includes_extra_dirs) do
            cppflags = cppflags .. ' -I' .. obj.staging .. v
        end
    end

    if (args.cache_configure ~= 'no') then
        table.insert (conf_opts, '--config-cache')
    end

    if (args.shared_libs == 'no') then
        table.insert (conf_opts, '--disable-shared')
    else
        table.insert(conf_opts, '--enable-shared')
    end
    if (args.static_libs == 'yes') then
        table.insert(conf_opts, '--enable-static')
    else
        table.insert (conf_opts, '--disable-static')
    end

    local conf_env = {
        'CFLAGS=' .. cflags,
        'CPPFLAGS=' .. cppflags,
        'CXXFLAGS=' .. cflags,
        'LDFLAGS=' .. ldflags,
        'LIBS=' .. libs,
        'PKG_CONFIG=/usr/bin/pkg-config'
         .. ' --define-variable=prefix=' .. obj.staging .. '/usr'
         .. ' --define-variable=libdir=' .. obj.staging .. '/usr/lib',
        'PKG_CONFIG_PATH=' .. obj.staging .. '/usr/lib/pkgconfig/',
    }

    if (args.config_args) then
        for _,v in pairs(args.config_args) do
            table.insert(conf_opts, v)
        end
    end

    if (args.config_env) then
        for _,v in pairs(args.config_env) do
            table.insert(conf_env, v)
        end
    end

    obj:cmd(builddir, rel_sourcedir .. 'configure', conf_opts, conf_env)

    -- build
    local make_args = {}
    if (args.make_job_limit) then
        table.insert(make_args, '-j' .. args.make_job_limit)
    elseif feature('job-limit') then
        table.insert(make_args, '-j' .. feature('job-limit'))
    else
        table.insert(make_args, '-j')
    end
    if feature('load-limit') then
        table.insert(make_args, '-l' .. feature('load-limit'))
    end
    if feature('verbose') then
        table.insert(make_args, 'V=' .. feature('verbose'))
    end
    table.insert(make_args, 'STAGING_DIR=' .. obj.staging)

    if (args.make_args) then
        for _,v in pairs(args.make_args) do
            table.insert(make_args, v)
        end
    end

    obj:make(builddir, make_args)

    -- install
    local install_env = {}

    if (args.install_env) then
        for _,v in pairs(args.install_env) do
            table.insert(install_env, v)
        end
    end

    if (args.install_install ~= 'no') then
        obj:make(builddir,{'DESTDIR=' .. obj.new_install,'install'}, install_env)
        local path = string.sub(prefix,2)
        if (path == '' or path == nil) then
            path = './'
        else
            path = path..'/'
        end
        -- Find all the la files in the new/install/*/lib and delete them. Some packages don't
        -- have lib dir e.g: gzip, so don't let the build fail due to that just let the build
        -- continue.
        obj:shell (obj.new_install, 'find '..path..'lib/ -name *.la -exec rm -f {} + || true')
        obj:shell (obj.new_install, 'find '..path..'lib64/ -name *.la -exec rm -f {} + || true')
    end
    if (args.install_staging == 'yes') then
        obj:make(builddir,{'DESTDIR=' .. obj.new_staging,'install'}, install_env)
        if (prefix == '/') then
            obj:mkdir (obj.new_staging, {'usr'})
            obj:mv (obj.new_staging, 'lib', 'usr/')
            obj:mv (obj.new_staging, 'include', 'usr/')
        end
        -- Find all the la files in the new/staging/*/lib and delete them.
        obj:shell (obj.new_staging, 'find usr/lib/ -name *.la -exec rm -f {} +')
        obj:shell (obj.new_staging, 'find usr/lib64/ -name *.la -exec rm -f {} + || true')
    end
    if (args.static_libs == 'no') then
        local path = string.sub(prefix,2)
        if (path == '' or path == nil) then
            path = './'
        else
            path = path..'/'
        end
        -- Find all the a files in the new/install/*/lib and delete them.
        obj:shell (obj.new_install, 'find '..path..'lib/ -name *.a -exec rm -f {} +')
    end
    return obj
end
