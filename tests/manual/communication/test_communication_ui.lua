if has_config("ui_test") and has_config("enable_communication") then 
    add_requires("qtbase")
    
    target("test_communication_ui")
        add_rules("qt.widgetapp")
        set_default(false)
        set_languages("c++20")
        add_packages("ilias")
        add_includedirs("$(projectdir)/include")
        add_files("test_communication_ui.cpp")
        add_files("test_communication_ui.hpp")
        add_files("test_communication_ui.ui")
        add_files("$(projectdir)/src/proto_base.cpp")
        add_files("$(projectdir)/src/communication_base.cpp")
        add_defines("NEKO_PROTO_STATIC")
        add_frameworks("QtCore", "QtNetwork", "QtWidgets", "QtGui")
    target_end()
end 