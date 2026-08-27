-- Compile-time benchmark xmake task comparing NekoProto and reflect-cpp

task("bench_compile_time")
    set_category("benchmark")
    set_menu({
        usage = "xmake bench_compile_time [options]",
        description = "Run compile-time benchmarks and compare NekoProto with reflect-cpp",
        options = {
            {'c', "compiler", "kv", nil,     "Path or name of compiler (default: clang++ or g++)"},
            {'r', "repeats",  "kv", "3",     "Number of iterations to average per workload (default: 3)"},
            {'f', "filter",   "kv", nil,     "Filter workload names by substring or pattern (e.g. 'json', 'yaml')"},
            {'o', "output",   "kv", nil,     "Output directory for benchmark results (default: build/compile_time/<label>)"},
            {'l', "label",    "kv", "local", "Label for current run (default: local)"},
            {nil, "trace",    "k",  nil,     "Enable Clang -ftime-trace profiling (requires clang++)"},
            {nil, "save",     "k",  nil,     "Save summary table to tests/manual/benchmarks/compile_time/RESULTS.md"}
        }
    })

    on_run(function ()
        import("core.base.option")
        import("core.project.config")
        import("core.project.project")
        import("lib.detect.find_tool")

        -- 1. Initialize project config
        config.load()

        local root = os.projectdir()
        local bench_dir = path.join(root, "tests", "manual", "benchmarks", "compile_time")
        local label = option.get("label") or "local"
        local repeats = tonumber(option.get("repeats") or "3") or 3
        local filter_pat = option.get("filter")
        local is_verbose = option.get("verbose")
        local enable_trace = option.get("trace")
        local save_results = option.get("save")

        local output_root = option.get("output") or path.join(root, "build", "compile_time", label)
        os.mkdir(output_root)

        -- 2. Detect Compiler
        local compiler = option.get("compiler")
        if not compiler or compiler == "" then
            local clang = find_tool("clang++")
            if clang and clang.program then
                compiler = clang.program
            else
                local gxx = find_tool("g++")
                if gxx and gxx.program then
                    compiler = gxx.program
                else
                    compiler = "clang++"
                end
            end
        end

        local is_clang = compiler:find("clang", 1, true) ~= nil
        local is_msvc = compiler:find("cl", 1, true) ~= nil and compiler:find("clang", 1, true) == nil

        -- Get compiler version
        local compiler_version = "unknown"
        try {
            function ()
                local out = os.iorunv(compiler, {"--version"})
                if out then
                    local first_line = out:split("\n")[1]
                    compiler_version = first_line or "unknown"
                end
            end,
            catch {
                function ()
                    compiler_version = compiler
                end
            }
        }

        -- 3. Resolve include directories
        local pkg_dir = config.get("packagedir") or path.join(os.getenv("HOME") or os.getenv("USERPROFILE") or "", ".xmake", "packages")

        local function resolve_inc(dir)
            if not dir then return nil end
            local inc = path.join(dir, "include")
            if os.isdir(inc) then return inc end
            if os.isdir(dir) then return dir end
            return nil
        end

        local function find_package_include(pkg_name, alt_name)
            -- 1. Try orderpkgs from NekoSerializer target
            project.load_targets()
            local target = project.target("NekoSerializer")
            if target then
                for _, pkg in ipairs(target:orderpkgs()) do
                    local n = pkg:name()
                    if n == pkg_name or (alt_name and n == alt_name) then
                        local inc = pkg:installdir("include")
                        local resolved = resolve_inc(inc)
                        if resolved then return resolved end
                    end
                end
            end

            -- 2. Search pkg_dir
            local names = {pkg_name}
            if alt_name then table.insert(names, alt_name) end
            for _, name in ipairs(names) do
                local sub = name:sub(1, 1):lower()
                local pattern = path.join(pkg_dir, sub, name, "*", "*")
                local dirs = os.dirs(pattern)
                if dirs and #dirs > 0 then
                    table.sort(dirs)
                    local resolved = resolve_inc(dirs[#dirs])
                    if resolved then return resolved end
                end
            end
            return nil
        end

        local inc_fmt = find_package_include("fmt")
        local inc_rapidjson = find_package_include("rapidjson")
        local inc_simdjson = find_package_include("simdjson")
        local inc_pugixml = find_package_include("pugixml")
        local inc_libfyaml = find_package_include("libfyaml")
        local inc_toml = find_package_include("toml++", "tomlplusplus")
        local inc_yamlcpp = find_package_include("yaml-cpp", "yamlcpp")
        local inc_rfl = find_package_include("reflect-cpp", "reflectcpp")

        -- Auto-install reflect-cpp if missing
        if not inc_rfl then
            cprint("${yellow}[benchmark] reflect-cpp not found in cache. Attempting auto-install via xrepo...${clear}")
            try {
                function ()
                    os.execv("xrepo", {"install", "-y", "reflect-cpp"})
                    inc_rfl = find_package_include("reflect-cpp", "reflectcpp")
                end
            }
        end

        -- Print Banner
        cprint("${cyan}========================================================================================${clear}")
        cprint("${bright cyan}                    NekoProtoTools Compile-Time Benchmark Suite                         ${clear}")
        cprint("${cyan}========================================================================================${clear}")
        cprint("Compiler       : %s", compiler_version)
        cprint("Platform       : %s (%s)", os.host(), os.arch())
        cprint("Output Path    : %s", output_root)
        cprint("Repeats        : %d iterations per workload", repeats)
        cprint("Trace Profile  : %s", enable_trace and "Enabled (-ftime-trace)" or "Disabled")
        cprint("reflect-cpp Inc: %s", inc_rfl or "${red}NOT FOUND (reflect-cpp benchmarks skipped)${clear}")
        cprint("${cyan}----------------------------------------------------------------------------------------${clear}")

        -- 4. Workload Definitions
        -- Base compiler flags
        local std_flag = "-std=c++23"
        local neko_common_flags = {std_flag, "-O0", "-c", "-I" .. path.join(root, "include"), "-I" .. bench_dir}
        if inc_fmt then table.insert(neko_common_flags, "-I" .. inc_fmt) end
        if enable_trace and is_clang then table.insert(neko_common_flags, "-ftime-trace") end

        local rfl_common_flags = {"-std=c++20", "-O0", "-c", "-I" .. bench_dir}
        if inc_rfl then table.insert(rfl_common_flags, "-I" .. inc_rfl) end
        if enable_trace and is_clang then table.insert(rfl_common_flags, "-ftime-trace") end

        -- Comparative cases (Neko vs reflect-cpp)
        local comp_cases = {
            {
                name = "include_cost",
                desc = "Core header include overhead",
                neko_source = "include_reflection.cpp",
                rfl_source  = "rfl_include.cpp",
                neko_flags  = {},
                rfl_flags   = {}
            },
            {
                name = "json_write_4",
                desc = "JSON write (4 fields)",
                neko_source = "json_write.cpp",
                rfl_source  = "rfl_json_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=4", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=4"}
            },
            {
                name = "json_read_4",
                desc = "JSON read (4 fields)",
                neko_source = "json_read.cpp",
                rfl_source  = "rfl_json_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=4", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=4"}
            },
            {
                name = "json_write_16",
                desc = "JSON write (16 fields)",
                neko_source = "json_write.cpp",
                rfl_source  = "rfl_json_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=16", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=16"}
            },
            {
                name = "json_read_16",
                desc = "JSON read (16 fields)",
                neko_source = "json_read.cpp",
                rfl_source  = "rfl_json_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=16", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=16"}
            },
            {
                name = "json_write_32",
                desc = "JSON write (32 fields)",
                neko_source = "json_write.cpp",
                rfl_source  = "rfl_json_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=32", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=32"}
            },
            {
                name = "json_read_32",
                desc = "JSON read (32 fields)",
                neko_source = "json_read.cpp",
                rfl_source  = "rfl_json_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=32", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=32"}
            },
            {
                name = "json_write_64",
                desc = "JSON write (64 fields)",
                neko_source = "json_write.cpp",
                rfl_source  = "rfl_json_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=64", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64"}
            },
            {
                name = "json_read_64",
                desc = "JSON read (64 fields)",
                neko_source = "json_read.cpp",
                rfl_source  = "rfl_json_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_RAPIDJSON", "-DNEKO_BENCH_FIELD_COUNT=64", inc_rapidjson and ("-I" .. inc_rapidjson) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64"}
            },
            {
                name = "yaml_write_64",
                desc = "YAML write (64 fields)",
                neko_source = "yaml_write.cpp",
                rfl_source  = "rfl_yaml_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_LIBFYAML", "-DNEKO_BENCH_FIELD_COUNT=64", inc_libfyaml and ("-I" .. inc_libfyaml) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64", inc_yamlcpp and ("-I" .. inc_yamlcpp) or ""}
            },
            {
                name = "yaml_read_64",
                desc = "YAML read (64 fields)",
                neko_source = "yaml_read.cpp",
                rfl_source  = "rfl_yaml_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_LIBFYAML", "-DNEKO_BENCH_FIELD_COUNT=64", inc_libfyaml and ("-I" .. inc_libfyaml) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64", inc_yamlcpp and ("-I" .. inc_yamlcpp) or ""}
            },
            {
                name = "toml_write_64",
                desc = "TOML write (64 fields)",
                neko_source = "toml_write.cpp",
                rfl_source  = "rfl_toml_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_TOMLPLUSPLUS", "-DNEKO_BENCH_FIELD_COUNT=64", inc_toml and ("-I" .. inc_toml) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64", inc_toml and ("-I" .. inc_toml) or ""}
            },
            {
                name = "toml_read_64",
                desc = "TOML read (64 fields)",
                neko_source = "toml_read.cpp",
                rfl_source  = "rfl_toml_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_TOMLPLUSPLUS", "-DNEKO_BENCH_FIELD_COUNT=64", inc_toml and ("-I" .. inc_toml) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64", inc_toml and ("-I" .. inc_toml) or ""}
            },
            {
                name = "xml_write_64",
                desc = "XML write (64 fields)",
                neko_source = "xml_write.cpp",
                rfl_source  = "rfl_xml_write.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_PUGIXML", "-DNEKO_BENCH_FIELD_COUNT=64", inc_pugixml and ("-I" .. inc_pugixml) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64", inc_pugixml and ("-I" .. inc_pugixml) or ""}
            },
            {
                name = "xml_read_64",
                desc = "XML read (64 fields)",
                neko_source = "xml_read.cpp",
                rfl_source  = "rfl_xml_read.cpp",
                neko_flags  = {"-DNEKO_PROTO_ENABLE_PUGIXML", "-DNEKO_BENCH_FIELD_COUNT=64", inc_pugixml and ("-I" .. inc_pugixml) or ""},
                rfl_flags   = {"-DNEKO_BENCH_FIELD_COUNT=64", inc_pugixml and ("-I" .. inc_pugixml) or ""}
            }
        }

        -- NekoProto-specific feature benchmarks
        local neko_feature_cases = {
            {
                name = "binary_write_64",
                desc = "Neko binary write (64 fields)",
                source = "binary_write.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=64", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "binary_read_64",
                desc = "Neko binary read (64 fields)",
                source = "binary_read.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=64", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "foreach_4",
                desc = "Reflect::forEach (4 fields)",
                source = "foreach.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=4", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "foreach_16",
                desc = "Reflect::forEach (16 fields)",
                source = "foreach.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=16", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "foreach_32",
                desc = "Reflect::forEach (32 fields)",
                source = "foreach.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=32", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "foreach_64",
                desc = "Reflect::forEach (64 fields)",
                source = "foreach.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=64", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "foreach_64_sparse",
                desc = "Reflect::forEach (64 fields, 1 tag/field)",
                source = "foreach.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=64", "-DNEKO_BENCH_TAG_DENSITY=1"}
            },
            {
                name = "foreach_64_dense",
                desc = "Reflect::forEach (64 fields, 4 tags/field)",
                source = "foreach.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=64", "-DNEKO_BENCH_TAG_DENSITY=2"}
            },
            {
                name = "schema_64",
                desc = "Runtime schema generation (64 fields)",
                source = "schema.cpp",
                flags = {"-DNEKO_BENCH_FIELD_COUNT=64", "-DNEKO_BENCH_TAG_DENSITY=0"}
            },
            {
                name = "enum_heavy",
                desc = "Constexpr enum tables (10 enums)",
                source = "enum_heavy.cpp",
                flags = {}
            }
        }

        -- Helper to compile a single workload
        local function run_compile_benchmark(source_file, base_flags, extra_flags, out_obj_name)
            local args = {}
            for _, f in ipairs(base_flags) do
                if f and f ~= "" then table.insert(args, f) end
            end
            for _, f in ipairs(extra_flags) do
                if f and f ~= "" then table.insert(args, f) end
            end
            table.insert(args, path.join(bench_dir, source_file))
            local obj_file = path.join(output_root, out_obj_name .. ".o")
            table.insert(args, "-o")
            table.insert(args, obj_file)

            if is_verbose then
                print("[CMD]", compiler, table.concat(args, " "))
            end

            local durations = {}
            local final_size = 0
            for i = 1, repeats do
                local t0 = os.mclock()
                local ok = true
                try {
                    function ()
                        os.iorunv(compiler, args)
                    end,
                    catch {
                        function (err)
                            ok = false
                            cprint("${red}Compilation failed for %s: %s${clear}", source_file, tostring(err):sub(1, 200))
                        end
                    }
                }
                local t1 = os.mclock()
                if not ok then
                    return nil
                end
                table.insert(durations, t1 - t0)
            end

            if os.isfile(obj_file) then
                final_size = os.filesize(obj_file)
            end

            local sum = 0
            local min_t = durations[1] or 0
            local max_t = durations[1] or 0
            for _, d in ipairs(durations) do
                sum = sum + d
                if d < min_t then min_t = d end
                if d > max_t then max_t = d end
            end
            local avg_t = sum / #durations

            return {
                avg_ms = avg_t,
                min_ms = min_t,
                max_ms = max_t,
                size_bytes = final_size
            }
        end

        local function format_size(bytes)
            if not bytes or bytes == 0 then return "N/A" end
            if bytes >= 1024 * 1024 then
                return string.format("%.2f MB", bytes / (1024 * 1024))
            elseif bytes >= 1024 then
                return string.format("%.1f KB", bytes / 1024)
            else
                return string.format("%d B", bytes)
            end
        end

        -- 5. Execute Comparative Benchmarks
        cprint("\n${yellow}>>> [1/2] Head-to-Head Comparison: NekoProto vs reflect-cpp${clear}\n")

        local comp_results = {}
        for _, c in ipairs(comp_cases) do
            if not filter_pat or c.name:find(filter_pat) or c.desc:find(filter_pat) then
                printf("Running %-22s ... ", c.name)
                io.flush()

                local neko_res = run_compile_benchmark(c.neko_source, neko_common_flags, c.neko_flags, "neko_" .. c.name)
                local rfl_res = nil
                if inc_rfl then
                    rfl_res = run_compile_benchmark(c.rfl_source, rfl_common_flags, c.rfl_flags, "rfl_" .. c.name)
                end

                if neko_res then
                    local rfl_str = rfl_res and string.format("%.1f ms", rfl_res.avg_ms) or "N/A"
                    local delta_str = "N/A"
                    local speedup_color = ""
                    if rfl_res and rfl_res.avg_ms > 0 then
                        local diff_pct = ((rfl_res.avg_ms - neko_res.avg_ms) / rfl_res.avg_ms) * 100.0
                        local speedup_x = rfl_res.avg_ms / neko_res.avg_ms
                        if diff_pct > 0 then
                            delta_str = string.format("+%.1f%% (%.2fx)", diff_pct, speedup_x)
                            speedup_color = "${green}"
                        else
                            delta_str = string.format("%.1f%% (%.2fx)", diff_pct, speedup_x)
                            speedup_color = "${magenta}"
                        end
                    end
                    cprint("Neko: ${cyan}%7.1f ms${clear} | rfl: ${yellow}%7s${clear} | Delta: " .. speedup_color .. "%s${clear}",
                           neko_res.avg_ms, rfl_str, delta_str)
                else
                    cprint("${red}FAILED${clear}")
                end

                table.insert(comp_results, {
                    name = c.name,
                    desc = c.desc,
                    neko = neko_res,
                    rfl  = rfl_res
                })
            end
        end

        -- 6. Execute NekoProto Feature Benchmarks
        cprint("\n${yellow}>>> [2/2] NekoProto Feature & Scaling Workloads${clear}\n")

        local feature_results = {}
        for _, c in ipairs(neko_feature_cases) do
            if not filter_pat or c.name:find(filter_pat) or c.desc:find(filter_pat) then
                printf("Running %-22s ... ", c.name)
                io.flush()
                local res = run_compile_benchmark(c.source, neko_common_flags, c.flags, "neko_" .. c.name)
                if res then
                    cprint("Time: ${cyan}%7.1f ms${clear} (min: %.1f, max: %.1f) | Obj: ${green}%s${clear}",
                           res.avg_ms, res.min_ms, res.max_ms, format_size(res.size_bytes))
                else
                    cprint("${red}FAILED${clear}")
                end
                table.insert(feature_results, {
                    name = c.name,
                    desc = c.desc,
                    res = res
                })
            end
        end

        -- 7. Output Final Formatted Report Table in Terminal
        cprint("\n")
        cprint("${cyan}========================================================================================================${clear}")
        cprint("${bright cyan}                               BENCHMARK RESULTS & COMPARISON REPORT                                     ${clear}")
        cprint("${cyan}========================================================================================================${clear}")
        printf("%-20s | %-12s | %-12s | %-18s | %-11s | %-11s\n",
               "Workload", "Neko Time", "rfl-cpp Time", "Speedup (Delta)", "Neko Obj", "rfl Obj")
        cprint("---------------------+--------------+--------------+--------------------+-------------+-------------")

        local total_neko_time = 0
        local total_rfl_time = 0
        local total_neko_obj = 0
        local total_rfl_obj = 0
        local comp_count = 0

        for _, r in ipairs(comp_results) do
            local neko_t = r.neko and string.format("%.1f ms", r.neko.avg_ms) or "ERR"
            local rfl_t  = r.rfl and string.format("%.1f ms", r.rfl.avg_ms) or "N/A"
            local neko_s = r.neko and format_size(r.neko.size_bytes) or "ERR"
            local rfl_s  = r.rfl and format_size(r.rfl.size_bytes) or "N/A"

            if r.neko and r.rfl and r.rfl.avg_ms > 0 then
                total_neko_time = total_neko_time + r.neko.avg_ms
                total_rfl_time = total_rfl_time + r.rfl.avg_ms
                total_neko_obj = total_neko_obj + (r.neko.size_bytes or 0)
                total_rfl_obj = total_rfl_obj + (r.rfl.size_bytes or 0)
                comp_count = comp_count + 1
            end

            local delta = "N/A"
            local delta_c = ""
            if r.neko and r.rfl and r.rfl.avg_ms > 0 then
                local pct = ((r.rfl.avg_ms - r.neko.avg_ms) / r.rfl.avg_ms) * 100.0
                local x = r.rfl.avg_ms / r.neko.avg_ms
                if pct >= 0 then
                    delta = string.format("+%5.1f%% (%4.2fx)", pct, x)
                    delta_c = "${green}"
                else
                    delta = string.format("%5.1f%% (%4.2fx)", pct, x)
                    delta_c = "${magenta}"
                end
            end

            local line_head = string.format("%-20s | %12s | %12s | ", r.name, neko_t, rfl_t)
            local line_delta = string.format("%-18s", delta)
            local line_tail = string.format(" | %11s | %11s", neko_s, rfl_s)
            cprint("%s" .. delta_c .. "%s${clear}%s", line_head, line_delta, line_tail)
        end

        cprint("---------------------+--------------+--------------+--------------------+-------------+-------------")
        if comp_count > 0 then
            local overall_pct = ((total_rfl_time - total_neko_time) / total_rfl_time) * 100.0
            local overall_x = total_rfl_time / total_neko_time
            local total_neko_t_str = string.format("%.1f ms", total_neko_time)
            local total_rfl_t_str = string.format("%.1f ms", total_rfl_time)
            local overall_delta = string.format("+%5.1f%% (%4.2fx)", overall_pct, overall_x)
            local total_neko_s_str = format_size(total_neko_obj)
            local total_rfl_s_str = format_size(total_rfl_obj)

            local sum_head = string.format("%-20s | %12s | %12s | ", "TOTAL (ALL FORMATS)", total_neko_t_str, total_rfl_t_str)
            local sum_delta = string.format("%-18s", overall_delta)
            local sum_tail = string.format(" | %11s | %11s", total_neko_s_str, total_rfl_s_str)
            cprint("${cyan}%s${green}%s${cyan}%s${clear}", sum_head, sum_delta, sum_tail)
            cprint("---------------------+--------------+--------------+--------------------+-------------+-------------")
        end

        cprint("\n${cyan}NekoProto Extended Feature Workloads:${clear}")
        cprint("--------------------------------------------------------------------------------------------------------")
        printf("%-26s | %-40s | %-12s | %-12s\n", "Workload", "Description", "Compile Time", "Object Size")
        cprint("---------------------------+------------------------------------------+--------------+--------------")
        for _, f in ipairs(feature_results) do
            local t_str = f.res and string.format("%.1f ms", f.res.avg_ms) or "ERR"
            local s_str = f.res and format_size(f.res.size_bytes) or "ERR"
            printf("%-26s | %-40s | %12s | %12s\n", f.name, f.desc, t_str, s_str)
        end
        cprint("========================================================================================================\n")

        -- 8. Generate and Export CSV & Markdown Artifacts
        local csv_file = path.join(output_root, "timings.csv")
        local f_csv = io.open(csv_file, "w")
        if f_csv then
            f_csv:write("category,workload,description,neko_avg_ms,neko_min_ms,neko_max_ms,neko_size_bytes,rfl_avg_ms,rfl_min_ms,rfl_max_ms,rfl_size_bytes,speedup_pct,speedup_ratio\n")
            for _, r in ipairs(comp_results) do
                local n_avg = r.neko and string.format("%.2f", r.neko.avg_ms) or ""
                local n_min = r.neko and string.format("%.2f", r.neko.min_ms) or ""
                local n_max = r.neko and string.format("%.2f", r.neko.max_ms) or ""
                local n_sz  = r.neko and tostring(r.neko.size_bytes) or ""
                local r_avg = r.rfl and string.format("%.2f", r.rfl.avg_ms) or ""
                local r_min = r.rfl and string.format("%.2f", r.rfl.min_ms) or ""
                local r_max = r.rfl and string.format("%.2f", r.rfl.max_ms) or ""
                local r_sz  = r.rfl and tostring(r.rfl.size_bytes) or ""
                local spd_pct = ""
                local spd_rat = ""
                if r.neko and r.rfl and r.rfl.avg_ms > 0 then
                    spd_pct = string.format("%.2f", ((r.rfl.avg_ms - r.neko.avg_ms) / r.rfl.avg_ms) * 100.0)
                    spd_rat = string.format("%.2f", r.rfl.avg_ms / r.neko.avg_ms)
                end
                f_csv:write(string.format("comparative,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                    r.name, r.desc, n_avg, n_min, n_max, n_sz, r_avg, r_min, r_max, r_sz, spd_pct, spd_rat))
            end
            for _, f in ipairs(feature_results) do
                local n_avg = f.res and string.format("%.2f", f.res.avg_ms) or ""
                local n_min = f.res and string.format("%.2f", f.res.min_ms) or ""
                local n_max = f.res and string.format("%.2f", f.res.max_ms) or ""
                local n_sz  = f.res and tostring(f.res.size_bytes) or ""
                f_csv:write(string.format("feature,%s,%s,%s,%s,%s,%s,,,,,\n",
                    f.name, f.desc, n_avg, n_min, n_max, n_sz))
            end
            f_csv:close()
            cprint("${green}Wrote timing dataset to: %s${clear}", csv_file)
        end

        -- Optionally update RESULTS.md
        if save_results then
            local results_md = path.join(bench_dir, "RESULTS.md")
            local f_md = io.open(results_md, "w")
            if f_md then
                f_md:write(string.format("# Reflection & Serialization Compile-Time Benchmark Results\n\n"))
                f_md:write(string.format("Environment: %s (%s), Compiler: `%s`, Optimization: `-O0`, Repeats: %d\n\n",
                           os.host(), os.arch(), compiler_version, repeats))
                f_md:write("## Head-to-Head Comparison: NekoProto vs reflect-cpp\n\n")
                f_md:write("| Workload | Description | NekoProto (ms) | reflect-cpp (ms) | Speedup (%) | Speedup (x) | Neko Obj | reflect-cpp Obj |\n")
                f_md:write("|:---|:---|---:|---:|---:|---:|---:|---:|\n")
                for _, r in ipairs(comp_results) do
                    local n_t = r.neko and string.format("%.1f", r.neko.avg_ms) or "ERR"
                    local r_t = r.rfl and string.format("%.1f", r.rfl.avg_ms) or "N/A"
                    local n_s = r.neko and format_size(r.neko.size_bytes) or "ERR"
                    local r_s = r.rfl and format_size(r.rfl.size_bytes) or "N/A"
                    local pct_s = "N/A"
                    local rat_s = "N/A"
                    if r.neko and r.rfl and r.rfl.avg_ms > 0 then
                        local pct = ((r.rfl.avg_ms - r.neko.avg_ms) / r.rfl.avg_ms) * 100.0
                        pct_s = string.format("%+.1f%%", pct)
                        rat_s = string.format("%.2fx", r.rfl.avg_ms / r.neko.avg_ms)
                    end
                    f_md:write(string.format("| `%s` | %s | %s | %s | %s | %s | %s | %s |\n",
                        r.name, r.desc, n_t, r_t, pct_s, rat_s, n_s, r_s))
                end

                if comp_count > 0 then
                    local overall_pct = ((total_rfl_time - total_neko_time) / total_rfl_time) * 100.0
                    local overall_x = total_rfl_time / total_neko_time
                    f_md:write(string.format("| **TOTAL (ALL FORMATS)** | Cumulative Benchmark Sum | **%.1f** | **%.1f** | **%+.1f%%** | **%.2fx** | **%s** | **%s** |\n",
                        total_neko_time, total_rfl_time, overall_pct, overall_x, format_size(total_neko_obj), format_size(total_rfl_obj)))
                end

                f_md:write("\n## NekoProto Feature & Scaling Workloads\n\n")
                f_md:write("| Workload | Description | Compile Time (ms) | Object Size |\n")
                f_md:write("|:---|:---|---:|---:|\n")
                for _, f in ipairs(feature_results) do
                    local t_s = f.res and string.format("%.1f", f.res.avg_ms) or "ERR"
                    local s_s = f.res and format_size(f.res.size_bytes) or "ERR"
                    f_md:write(string.format("| `%s` | %s | %s | %s |\n", f.name, f.desc, t_s, s_s))
                end
                f_md:close()
                cprint("${green}Updated benchmark report at: %s${clear}", results_md)
            end
        end
    end)
