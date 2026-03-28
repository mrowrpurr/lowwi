add_rules("mode.debug", "mode.release")
set_languages("c++23")

option("build_examples")
    set_default(true)
    set_showmenu(true)
    set_description("Build example binaries")
option_end()

add_requires("onnxruntime")
if has_config("build_examples") then
    add_requires("miniaudio")
end

target("lowwi")
    set_kind("static")
    add_files("src/*.cpp")
    add_headerfiles("src/*.hpp", "src/*.h")
    add_includedirs("src", {public = true})
    add_packages("onnxruntime", {public = true})

if has_config("build_examples") then
target("mic")
    set_kind("binary")
    add_files("example/mic/*.cpp")
    add_deps("lowwi")
    add_packages("miniaudio")

    after_build(function (target)
        -- Copy ORT DLLs next to the executable
        local ort = target:dep("lowwi"):pkg("onnxruntime")
        if ort then
            for _, dll in ipairs(os.files(path.join(ort:installdir(), "bin", "*.dll"))) do
                os.cp(dll, target:targetdir())
            end
        end
        -- Copy bundled models next to the executable
        os.cp("$(projectdir)/models", path.join(target:targetdir(), "models"))
    end)
end
