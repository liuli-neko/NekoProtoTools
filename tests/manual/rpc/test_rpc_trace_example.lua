if has_config("enable_rpc_trace") then
target("test_rpc_trace_example")
    set_kind("binary")
    set_default(false)
    add_includedirs("$(projectdir)/include")
    add_deps("NekoJsonRpc", "NekoSerializer", "NekoArgParser")
    add_files("test_rpc_trace_example.cpp", "$(projectdir)/src/rpc_tracing.cpp")
    add_defines("NEKO_PROTO_STATIC")
    add_options("enable_rpc_trace")
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target, {uses_ilias = true})
    end)
target_end()
end

