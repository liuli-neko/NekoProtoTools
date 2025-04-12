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
