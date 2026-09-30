task("bench_json")
    set_category("benchmark")
    set_menu({
        usage = "xmake bench_json [options]",
        description = "Build and run the three-library JSON comparison",
        options = {
            {'n', "iterations", "kv", "10000", "Operations per case (default: 10000)"},
            {'r', "repeats", "kv", "3", "Complete runs per library (default: 3)"},
            {'o', "output", "kv", nil, "Output directory (default: build/benchmarks/json)"}
        }
    })
    on_run(function ()
        import("core.base.option")
        import("core.project.config")
        import("core.project.project")
        import("core.tool.compiler")
        config.load()
        local iterations = tonumber(option.get("iterations"))
        local repeats = tonumber(option.get("repeats"))
        assert(iterations and iterations > 0 and iterations == math.floor(iterations), "iterations must be a positive integer")
        assert(repeats and repeats > 0 and repeats == math.floor(repeats), "repeats must be a positive integer")
        assert(config.get("mode") == "release" and config.get("enable_benchmarks"),
               "Configure first: xmake f -m release --enable_benchmarks=y")

        local output_dir = option.get("output") or path.join(os.projectdir(), "build", "benchmarks", "json")
        os.mkdir(output_dir)
        local targets = {"bench_json_rapidjson_raw", "bench_json_nekoproto", "bench_json_nekoproto_simdjson", "bench_json_reflect_cpp", "bench_json_glaze"}
        for _, name in ipairs(targets) do
            os.execv("xmake", {"build", name})
        end
        project.load_targets()

        local compile_commands = {}
        for _, name in ipairs(targets) do
            local target = assert(project.target(name))
            for _, batch in pairs(target:sourcebatches()) do
                for index, objectfile in ipairs(batch.objectfiles) do
                    table.insert(compile_commands, name .. ": " ..
                        compiler.compcmd(batch.sourcefiles[index], objectfile, {target = target}))
                end
            end
        end
        io.writefile(path.join(output_dir, "compile-commands.txt"),
                     table.concat(compile_commands, "\n") .. "\n")

        local resolved_packages = {}
        for _, name in ipairs(targets) do
            for _, pkg in ipairs(project.target(name):orderpkgs() or {}) do
                if pkg:name() == "rapidjson" or pkg:name() == "reflect-cpp" or pkg:name() == "glaze" or pkg:name() == "yyjson" or pkg:name() == "simdjson" then
                    resolved_packages[pkg:name()] = tostring(pkg:version() or "unknown")
                end
            end
        end
        local cases = {"small", "medium", "large", "small_null_email"}
        local fixture_dir = path.join(output_dir, "fixtures")
        os.mkdir(fixture_dir)
        for _, case_name in ipairs(cases) do
            local emitted = {}
            for _, name in ipairs(targets) do
                local value = os.iorunv(project.target(name):targetfile(), {"--emit", case_name})
                assert(#value > 0, name .. " emitted empty JSON")
                local filename = path.join(fixture_dir, case_name .. "-" .. name .. ".json")
                io.writefile(filename, value)
                table.insert(emitted, filename)
                if name == "bench_json_nekoproto" then
                    io.writefile(path.join(fixture_dir, case_name .. ".json"), value)
                end
            end
            for _, filename in ipairs(emitted) do
                for _, name in ipairs(targets) do
                    os.execv(project.target(name):targetfile(), {"--check-json", case_name, filename})
                end
            end
        end

        local raw_rows = {"repeat,case,library,backend,operation,iterations,json_bytes,ns_per_op,mb_per_second"}
        local header = "case,library,backend,operation,iterations,json_bytes,ns_per_op,mb_per_second"
        local libraries = {
            {name = "NekoProtoTools", backend = "yyjson", column = "neko_yyjson", label = "Neko / yyjson"},
            {name = "NekoProtoTools", backend = "simdjson", column = "neko_simdjson", label = "Neko / simdjson"},
            {name = "reflect-cpp", backend = "yyjson", column = "reflect_cpp", label = "reflect-cpp / yyjson"},
            {name = "Glaze", backend = "Glaze JSON", column = "glaze", label = "Glaze"},
            {name = "RapidJSON-Raw", backend = "Streaming", column = "rapidjson_raw", label = "RapidJSON (Handwritten)"}
        }
        local measurements = {}
        local function measurement(case_name, operation, column)
            local key = case_name .. "/" .. operation .. "/" .. column
            if not measurements[key] then
                measurements[key] = {ns = {}, mb = {}, bytes = nil}
            end
            return measurements[key]
        end
        for repeat_index = 1, repeats do
            -- Rotate the starting library to reduce fixed process-order bias.
            for offset = 0, #targets - 1 do
                local name = targets[((repeat_index + offset - 2) % #targets) + 1]
                local target = assert(project.target(name), "missing benchmark target: " .. name)
                local output = os.iorunv(target:targetfile(), {tostring(iterations), fixture_dir})
                local count = 0
                for line in output:gmatch("[^\r\n]+") do
                    if line ~= header then
                        local fields = {}
                        for field in line:gmatch("[^,]+") do table.insert(fields, field) end
                        assert(#fields == 8 and tonumber(fields[5]) == iterations,
                                "invalid benchmark CSV fields")
                        local known_case = false
                        for _, case_name in ipairs(cases) do
                            if fields[1] == case_name then known_case = true end
                        end
                        assert(known_case and (fields[4] == "serialize" or fields[4] == "deserialize"),
                                "invalid benchmark case or operation")
                        local library
                        for _, candidate in ipairs(libraries) do
                            if fields[2] == candidate.name and fields[3] == candidate.backend then
                                library = candidate
                                break
                            end
                        end
                        assert(library, "unexpected benchmark library/backend")
                        local entry = measurement(fields[1], fields[4], library.column)
                        local bytes = assert(tonumber(fields[6]), "invalid JSON byte count")
                        assert(not entry.bytes or entry.bytes == bytes, "JSON byte count changed across repeats")
                        entry.bytes = bytes
                        table.insert(entry.ns, assert(tonumber(fields[7]), "invalid ns/op"))
                        table.insert(entry.mb, assert(tonumber(fields[8]), "invalid MB/s"))
                        table.insert(raw_rows, tostring(repeat_index) .. "," .. line)
                        count = count + 1
                    end
                end
                assert(count == #cases * 2, name .. " did not produce all case rows")
            end
        end
        local function median(values)
            assert(#values == repeats, "missing benchmark repeat")
            table.sort(values)
            if repeats % 2 == 1 then return values[(repeats + 1) / 2] end
            return (values[repeats / 2] + values[repeats / 2 + 1]) / 2
        end
        local summary_ns_headers = {}
        local summary_mb_headers = {}
        local size_col_headers = {}
        local col_labels = {}
        local separators = {}
        for _, lib in ipairs(libraries) do
            table.insert(summary_ns_headers, lib.column .. "_ns_per_op")
            table.insert(summary_mb_headers, lib.column .. "_mb_per_second")
            table.insert(size_col_headers, lib.column .. "_bytes")
            table.insert(col_labels, lib.label)
            table.insert(separators, "---:")
        end
        local summary_rows = {
            "operation,case,iterations," .. table.concat(summary_ns_headers, ",") .. "," .. table.concat(summary_mb_headers, ",")
        }
        local size_rows = {"case," .. table.concat(size_col_headers, ",")}
        local table_header_line = "| Case | " .. table.concat(col_labels, " | ") .. " |"
        local table_sep_line = "|---|" .. table.concat(separators, "|") .. "|"
        local comparison_tables = {}
        for _, operation in ipairs({"serialize", "deserialize"}) do
            local latency_table = {
                "### ns/op (lower is better)", "",
                table_header_line,
                table_sep_line
            }
            local throughput_table = {
                "### MB/s (higher is better)", "",
                table_header_line,
                table_sep_line
            }
            for _, case_name in ipairs(cases) do
                local ns_values = {}
                local mb_values = {}
                for _, library in ipairs(libraries) do
                    local entry = assert(measurements[case_name .. "/" .. operation .. "/" .. library.column])
                    table.insert(ns_values, string.format("%.3f", median(entry.ns)))
                    table.insert(mb_values, string.format("%.3f", median(entry.mb)))
                end
                local values = {operation, case_name, tostring(iterations)}
                for _, value in ipairs(ns_values) do table.insert(values, value) end
                for _, value in ipairs(mb_values) do table.insert(values, value) end
                table.insert(summary_rows, table.concat(values, ","))
                table.insert(latency_table, "| " .. case_name .. " | " .. table.concat(ns_values, " | ") .. " |")
                table.insert(throughput_table, "| " .. case_name .. " | " .. table.concat(mb_values, " | ") .. " |")
            end
            table.insert(comparison_tables, "## " .. operation)
            table.insert(comparison_tables, "")
            table.insert(comparison_tables, table.concat(latency_table, "\n"))
            table.insert(comparison_tables, "")
            table.insert(comparison_tables, table.concat(throughput_table, "\n"))
            table.insert(comparison_tables, "")
        end
        local size_table = {
            "## Serialized JSON bytes", "",
            table_header_line,
            table_sep_line
        }
        for _, case_name in ipairs(cases) do
            local sizes = {}
            for _, library in ipairs(libraries) do
                local entry = assert(measurements[case_name .. "/serialize/" .. library.column])
                table.insert(sizes, tostring(entry.bytes))
            end
            table.insert(size_rows, case_name .. "," .. table.concat(sizes, ","))
            table.insert(size_table, "| " .. case_name .. " | " .. table.concat(sizes, " | ") .. " |")
        end
        local csv_path = path.join(output_dir, "json.csv")
        io.writefile(csv_path, table.concat(summary_rows, "\n") .. "\n")
        io.writefile(path.join(output_dir, "raw.csv"), table.concat(raw_rows, "\n") .. "\n")
        io.writefile(path.join(output_dir, "json_sizes.csv"), table.concat(size_rows, "\n") .. "\n")
        local report = {
            "# JSON benchmark comparison", "",
            "Median of " .. repeats .. " repeats, " .. iterations .. " operations per case. Each row compares the same case and operation across three adapters.",
            "", "The deserialize input is the same NekoProtoTools JSON fixture for every adapter. Serialize throughput uses each adapter's own output size.",
            "", table.concat(comparison_tables, "\n"), table.concat(size_table, "\n"), "",
            "[Comparison CSV](json.csv) · [Raw repeat data](raw.csv) · [Environment](environment.txt) · [Compile commands](compile-commands.txt)", ""
        }
        io.writefile(path.join(output_dir, "summary.md"), table.concat(report, "\n"))
        local xmake_version = os.iorunv("xmake", {"--version"}):gsub("\27%[[0-9;]*m", "")
        local metadata = {
            "JSON benchmark run",
            "xmake: " .. (xmake_version:match("[^\r\n]+") or "unknown"),
            "platform: " .. os.host() .. "/" .. os.arch(),
            "mode: release",
            "configured CXX: " .. tostring(config.get("cxx") or "xmake default"),
            "iterations per case: " .. iterations,
            "repeats: " .. repeats,
            "json.csv and summary.md: median of repeats; raw.csv: every measured repeat; json_sizes.csv: serialized output sizes",
            "NekoProtoTools: " .. (io.readfile(path.join(os.projectdir(), "version.lua")):match('return "([^"]+)"') or "unknown"),
            "yyjson: " .. tostring(resolved_packages["yyjson"] or "unknown"),
            "simdjson: " .. tostring(resolved_packages["simdjson"] or "unknown"),
            "RapidJSON: " .. tostring(resolved_packages["rapidjson"] or "unknown"),
            "reflect-cpp: " .. tostring(resolved_packages["reflect-cpp"] or "unknown") .. " (yyjson)",
            "Glaze: " .. tostring(resolved_packages["glaze"] or "unknown"),
            "command: xmake bench_json -n " .. iterations .. " -r " .. repeats .. " -o " .. output_dir,
            "model: benchmarks/benchmark_models.hpp; seeds: 0x4e454b4f20260400 and +4; warm-up: 100 per case",
            "All three outputs were decoded by all three adapters before timing.",
            "Deserialize uses the same NekoProtoTools JSON fixture for every adapter; serialize measures each native output.",
            "json_bytes means produced bytes for serialize, fixture input bytes for deserialize.",
            "Every case passed round-trip checks before and after measurement."
        }
        io.writefile(path.join(output_dir, "environment.txt"), table.concat(metadata, "\n") .. "\n")
        cprint("${green}JSON benchmark results: %s${clear}", csv_path)
    end)
task_end()
