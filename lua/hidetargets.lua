if is_plat("linux") then
    target("coverage-report")
        set_kind("phony")

        on_run(function (target)
            import("core.project.config")

            print("Generating coverage report...")

            local projectdir = os.projectdir()
            -- Respect `xmake f --buildir=...` so coverage can be collected in
            -- a clean directory instead of merging stale objects from other
            -- compilers, modes, or targets left in the default build tree.
            local builddir = path.absolute(config.builddir(), projectdir)
            local objdir = path.join(builddir, ".objs")
            local infofile = path.join(builddir, "coverage.info")
            local reportdir = path.join(builddir, "coverage-report")

            os.execv("lcov", {
                "--capture",
                "--directory", objdir,
                "--base-directory", projectdir,
                "--output-file", infofile,

                -- GCC/gcov 生成的“未执行 block 但行有 hit count”的异常，按 lcov 提示归零
                "--rc", "geninfo_unexecuted_blocks=1",

                -- 如果还遇到 function end line 推导问题，可以打开这个
                -- 这会牺牲部分 function 范围信息，但 line coverage 仍然可用
                "--rc", "derive_function_end_line=0",

                "--no-external",
                "--exclude", "*.xmake/packages/*",
                "--exclude", "*modules/Ilias/*",
                -- GCC 14 may emit constructor aliases with the same symbol
                -- but different declaration lines. lcov 2.x classifies that
                -- function metadata as inconsistent; line coverage remains
                -- valid and is the report's primary contract.
                -- Object directories from targets disabled in the current
                -- configuration can survive `xmake clean -a`. gcov skips
                -- incompatible compiler-version records and records whose
                -- source was removed; they must not abort collection for the
                -- current, valid targets.
                "--ignore-errors", "unused,inconsistent,version,source",
                "--filter", "range",
                -- 你原来的 */tests/test_* 匹配不到 tests/unit/proto/test_proto.cpp
                "--exclude", path.join(projectdir, "tests/*")
            })

            os.execv("genhtml", {
                "--output-directory", reportdir,
                "--title", "Coverage Report",
                -- GCC 14 constructor aliases and template instantiations can
                -- share a symbol while carrying different declaration lines.
                -- This does not invalidate line coverage.
                "--ignore-errors", "inconsistent,corrupt",
                infofile
            })
        end)
    target_end()
end

task("check")
    set_menu({
        usage = "xmake check",
        description = "Parallel syntax check with timing",
        options = {
            {nil, "target",  "v", "Check only the specified target."}
        }
    })
    
    on_run(function ()
        import("core.base.option")
        import("core.project.config")
        import("core.project.project")
        import("core.tool.compiler")
        import("async.runjobs")

        -- 1. 初始化
        config.load()
        project.lock()
        project.load_targets()
                
        local target_filter = option.get("target")
        local jobs = {}
        local checked_files = {} 
        
        -- 2. 收集任务
        for _, target in pairs(project.targets()) do
            if target_filter and target:name() == target_filter then
                if target:is_enabled() then
                    for _, sourcebatch in pairs(target:sourcebatches()) do
                        local sourcekind = sourcebatch.sourcekind
                        if sourcekind == "cxx" or sourcekind == "cc" then
                            for _, sourcefile in ipairs(sourcebatch.sourcefiles) do
                                if not checked_files[sourcefile] then
                                    checked_files[sourcefile] = true
                                    
                                    local extra_flags = {}
                                    if target:has_tool(sourcekind, "cl") then
                                        table.insert(extra_flags, "/Zs")
                                    else
                                        table.insert(extra_flags, "-fsyntax-only")
                                    end

                                    local compile_opts = { target = target }
                                    if sourcekind == "cxx" then
                                        compile_opts.cxxflags = extra_flags
                                    else
                                        compile_opts.cflags = extra_flags
                                    end

                                    -- 使用 build 目录下的 obj 路径，防止污染源码
                                    local obj_file = target:objectfile(sourcefile)
                                    
                                    table.insert(jobs, {
                                        source = sourcefile,
                                        obj = obj_file,
                                        opts = compile_opts
                                    })
                                end
                            end
                        end
                    end
                end
            end
        end

        local total = #jobs
        if total == 0 then
            cprint("${yellow}No source files to check.")
            return
        end

        cprint("${green}[xmake check]: Starting check for %d files...", total)

        -- >>> 计时开始 <<<
        local start_time = os.mclock()

        local count = 0
        local failed_files = {}

        -- 3. 并行执行
        runjobs("check_syntax", function (index)
            local item = jobs[index]
            local sourcefile = item.source
            local relpath = path.relative(sourcefile, os.projectdir())
            
            local ok = true
            local err_output = ""
            local t_start = os.mclock()

            try
            {
                function ()
                    compiler.compile(sourcefile, item.obj, item.opts)
                end,
                catch
                {
                    function (errors)
                        ok = false
                        err_output = errors
                    end
                }
            }

            -- 4. 实时输出
            count = count + 1
            local percent = math.floor((count / total) * 100)

            local duration = os.mclock() - t_start
            local time_str = string.format("(%dms)", duration)
            
            if ok then
                cprint("${dim}[%3d%%] ${green}Checked ${reset}${dim}%-40s ${default}%s", percent, relpath, time_str)
            else
                cprint("${dim}[%3d%%] ${red}Failed  ${reset}${bright}%-40s ${default}%s", percent, relpath, time_str)
                table.insert(failed_files, { file = relpath, err = err_output })
            end

        end, {total = total})

        -- >>> 计时结束 <<<
        local cost_time = os.mclock() - start_time
        local cost_str = string.format("%.2fs", cost_time / 1000)

        -- 5. 结果汇总
        if #failed_files > 0 then
            print("")
            cprint("${red}============================================================")
            cprint("${red}Check finished with %d errors (took %s):", #failed_files, cost_str)
            cprint("${red}============================================================")
            
            for _, item in ipairs(failed_files) do
                cprint("${yellow}> File: %s", item.file)
                print(item.err)
                print("")
            end
            raise("Syntax check failed!")
        else
            print("")
            -- 成功时显示耗时
            cprint("${bright green}✔ All %d files checked successfully in %s.", total, cost_str)
        end
    end)
task_end()
