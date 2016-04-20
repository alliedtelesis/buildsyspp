local _builddir = builddir

function builddir(arg)
    local obj = _builddir(arg)

    obj.sed = function(obj, dir, expr, files)
        local args = {'-i', '-e', expr}
        for _,v in pairs(files) do
            table.insert(args, v)
        end
        obj:cmd(dir, 'sed', args)
    end

    obj.mkdir = function(obj, dir, paths)
        local args = {'-p'}
        for _,v in pairs(paths) do
            table.insert(args, v)
        end
        obj:cmd(dir, 'mkdir', args)
    end

    obj.shell = function(obj, dir, command, env)
        local env = env or {}
        obj:cmd(dir, 'sh', {'-c', command}, env)
    end

    -- Make
    obj.make = function(obj, dir, options, env)
        local env = env or {}
        obj:cmd(dir, 'make', options, env)
    end

    -- Configure
    obj.configure = function(obj, dir, options, env)
        local env = env or {}
        obj:cmd(dir, './configure', options, env)
    end

    -- Copy
    obj.cp = function(obj, dir, options, src, dest)
        local args = {}

        -- Options
        if type(options) == "table" then
            for _,v in pairs(options) do
                table.insert(args, v)
            end
        else
            table.insert(args, options)
        end

        -- Source files
        if type(src) == "table" then
            for _,v in pairs(src) do
                table.insert(args, v)
            end
        else
            table.insert(args, src)
        end

        -- Destination
        table.insert(args, dest)
        obj:cmd(dir, 'cp', args)
    end

    -- Move
    obj.mv = function(obj, dir, options, src, dest)
        local args = {}

        -- Options
        if type(options) == "table" then
            for _,v in pairs (options) do
                table.insert(args, v)
            end
        else
            table.insert(args, options)
        end

        -- Source files
        if type(src) == "table" then
            for _,v in pairs (src) do
                table.insert(args, v)
            end
        else
            table.insert(args, src)
        end

        -- Destination
        table.insert(args, dest)
        obj:cmd(dir, 'mv', args)
    end

    -- Remove
    obj.rm = function(obj, dir, options, paths)
        local args = {}

        -- Options
        if type(options) == "table" then
            for _,v in pairs(options) do
                table.insert(args, v)
            end
        else
            table.insert(args, options)
        end

        -- Paths
        if type(paths) == "table" then
            for _,v in pairs(paths) do
                table.insert(args, v)
            end
        else
            table.insert(args, paths)
        end
        obj:cmd(dir, 'rm', args)
    end

    -- Link
    obj.ln = function (obj, dir, options, target, link)
        local args = {}

        -- Options
        if type(options) == "table" then
            for _,v in pairs(options) do
                table.insert(args, v)
            end
        else
            table.insert(args, options)
        end

        -- Target and link
        table.insert(args, target)
        table.insert(args, link)

        obj:cmd(dir, 'ln', args)
    end

    -- Change mode
    obj.chmod = function (obj, dir, options, paths)
        local args = {}

        -- Options
        if type(options) == "table" then
            for _,v in pairs(options) do
                table.insert(args, v)
            end
        else
            table.insert(args, options)
        end

        -- Paths
        if type(paths) == "table" then
            for _,v in pairs(paths) do
                table.insert(args, v)
            end
        else
            table.insert(args, paths)
        end

        obj:cmd(dir, 'chmod', args)
    end

    return obj
end
