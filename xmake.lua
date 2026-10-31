-- include subprojects
includes("lib/commonlibf4")

-- set project constants
set_project("AmmoTypeHUD")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("AmmoTypeHUD")
    add_rules("commonlibf4.plugin", {
        name = "AmmoTypeHUD",
        author = "jnappo",
        description = "Shows the ammo type next to the HUD ammo counter"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
