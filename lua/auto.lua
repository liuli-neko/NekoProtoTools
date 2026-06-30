local autofunc = autofunc or {}

import("core.project.project")

-- private

function _Camel(str)
    return str:sub(1, 1):upper() .. str:sub(2)
end

-- public

function autofunc.target_autoclean(target)
    os.tryrm(target:targetdir() .. "/" .. target:basename() .. ".exp")
    os.tryrm(target:targetdir() .. "/" .. target:basename() .. ".ilk")
    os.tryrm(target:targetdir() .. "/compile." .. target:basename() .. ".pdb")
    if is_plat("linux") then
        if target:kind() == "static" then
            os.tryrm(target:targetdir() .. "/lib" .. target:basename() .. ".so")
        elseif target:kind() == "shared" then
            os.tryrm(target:targetdir() .. "/lib" .. target:basename() .. ".a")
        end
    end
end

function autofunc.auto_add_packages(target, options)
    options = options or {}
    local uses_ilias = options.uses_ilias or false
    local uses_expected = options.uses_expected or false

    if has_config("enable_simdjson") then
        target:add("packages", "simdjson", {public = true})
        target:add("defines", "SIMDJSON_EXCEPTIONS=1")
    end
    
    if has_config("enable_rapidjson") then
        target:add("packages", "rapidjson", {public = true})
    end
    
    if has_config("enable_pugixml") then
        target:add("packages", "pugixml", {public = true})
    end
    
    if has_config("enable_spdlog") then
        target:add("packages", "spdlog", {public = true})
    end
    
    if has_config("enable_fmt") then
        target:add("packages", "fmt", {public = true})
    end

    if uses_expected and not has_config("has_std_expected") then
        target:add("packages", "zeus_expected", {public = true})
    end
    
    if uses_ilias then
        target:add("packages", "ilias", {public = true})
    end
    
    if has_config("enable_tests") then
        target:add("packages", "gtest", {public = true})
        target:add("ldflags", "-lgmock", {public = true, force = true})
        target:add("packages", "cpptrace", {public = true})
    end
    
    if has_config("cereal_test") then
        target:add("packages", "cereal", {public = true})
    end

    if uses_ilias and has_config("use_io_uring") then
        target:add("defines", "ILIAS_USE_IO_URING", {public = true})
    end

end

function main()
    return autofunc
end
