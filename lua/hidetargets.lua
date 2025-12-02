if is_plat("linux") then 
    target("coverage-report")
        set_kind("phony")
        set_default(true)
        -- 执行脚本命令
        on_run(function (target)
            print("Generating coverage report...")
            -- 在这里编写您想要执行的脚本命令
            os.execv("lcov ", {"--capture", 
                "--directory", os.projectdir() .. "/build/.objs/", 
                "--output-file", os.projectdir() .. "/build/coverage.info",
                "--exclude", "/usr/include/*",
                "--exclude", "/usr/local/include/*",
                "--exclude", "*.xmake/packages/*",
                "--exclude", "*modules/Ilias/*",
                "--exclude", "*/tests/test_*",
                "--directory", os.projectdir()})
            os.execv("genhtml ", {
                "--output-directory", os.projectdir() .. "/build/coverage-report",
                "--title", "'Coverage Report'", 
                os.projectdir() .. "/build/coverage.info"})
        end)
    target_end()
end 

task("check")
    set_menu({
        usage = "xmake check",
        description = "Parallel syntax check with timing",
        options = {}
    })
    
    on_run(function ()
        import("core.project.config")
        import("core.project.project")
        import("core.tool.compiler")
        import("async.runjobs")

        -- 1. 初始化
        config.load()
        project.lock()
        project.load_targets()
        
        local jobs = {}
        local checked_files = {} 
        
        -- 2. 收集任务
        for _, target in pairs(project.targets()) do
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