import os
import sys

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])

sources = Glob("src/*.cpp")

if env["target"] in ("debug", "dev", "development"):
    env.Append(CPPDEFINES=["DEBUG_ENABLED"])

if env.editor_build:
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])
    env.add_source_files(env.modules_sources, "./src/editor/*.cpp")

    SConscript("src/editor/script_templates/SCsub")

    if env["module_jsonrpc_enabled"] and env["module_websocket_enabled"]:
        env.add_source_files(env.modules_sources, "./src/language_server/*.cpp")
    else:
        env.Append(CPPDEFINES=["RUZTA_NO_LSP"])
        env.Append(CPPDEFINES=["RUZTA_NO_LSP"])

env["tests"] = ARGUMENTS.get("tests", "no") in ("yes", "true", "1")
if env["tests"]:
    env.Append(CPPDEFINES=["TESTS_ENABLED"])
    env.add_source_files(env.modules_sources, "./tests/*.cpp")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "demo/ruzta_lang/ruztalang.{}.{}.framework/ruztalang.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )

elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "demo/ruzta_lang/ruztalang.{}.{}.simulator.a".format(
                env["platform"], env["target"]
            ),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            "demo/ruzta_lang/ruztalang.{}.{}.a".format(env["platform"], env["target"]),
            source=sources,
        )

else:
    library = env.SharedLibrary(
        "demo/ruzta_lang/ruztalang{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
