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
end

function autofunc.auto_add_packages(target)
    if has_config("enable_simdjson") then
        target:add("packages", "simdjson", {public = true})
        target:add("defines", "SIMDJSON_EXCEPTIONS=1")
    end
    
    if has_config("enable_rapidjson") then
        target:add("packages", "rapidjson", {public = true})
    end
    
    if has_config("enable_rapidxml") then
        target:add("packages", "rapidxml", {public = true})
    end
    
    if has_config("enable_spdlog") then
        target:add("packages", "spdlog", {public = true})
    end
    
    if has_config("enable_fmt") then
        target:add("packages", "fmt", {public = true})
    end
    
    if has_config("enable_communication") or has_config("enable_jsonrpc") then
        target:add("packages", "ilias", {public = true})
    end
    
    if has_config("enable_tests") then
        target:add("packages", "gtest", {public = true})
    end
    
    if has_config("cereal_test") then
        target:add("packages", "cereal", {public = true})
    end

    if has_config("use_io_uring") then
        target:add("defines", "ILIAS_USE_IO_URING", {public = true})
    end

end

function main()
    return autofunc
end