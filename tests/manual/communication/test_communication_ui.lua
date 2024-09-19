if has_config("ui_test") and has_config("enable_communication") then 
    add_requires("qtbase")
    
    target("test_communication_ui")
        add_rules("qt.widgetapp")
        set_languages("c++20")
        add_includedirs("../../../modules/Ilias/include")
        add_includedirs("../../../")
        add_files("test_communication_ui.cpp")    
        add_files("test_communication_ui.hpp")    
        add_files("test_communication_ui.ui")    
        add_files("../../../src/proto_base.cpp")
        add_files("../../../src/communication_base.cpp")
        add_defines("NEKO_PROTO_STATIC")
        add_defines("ILIAS_COROUTINE_LIFETIME_CHECK")
        add_options("enable_spdlog", "enable_fmt", "enable_stdformat", "enable_rapidjson", "enable_simdjson")
        add_frameworks("QtCore", "QtNetwork", "QtWidgets", "QtGui")
    target_end()
end 