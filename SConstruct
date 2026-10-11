#!/usr/bin/env python
import os
import sys
from methods import print_error

libname = "FmodGodot"
so_number = 1

# FMOD's non-pthread HTML5 libraries only link into single-threaded side modules.
if ARGUMENTS.get("platform", "") == "web":
    ARGUMENTS.setdefault("threads", "no")

env = SConscript("godot-cpp/SConstruct")
source_path = [
    os.path.join("godot-cpp", "include", "godot_cpp"),
    os.path.join("godot-cpp", "gen", "include", "godot_cpp"),
]
# env.Append(CCFLAGS=["-Wall"])
# env.Append(CCFLAGS=["-Werror"])
logging = ""
if env["target"] in ["editor", "template_debug"]:
    logging = "L"

# Require C++17
if env.get("is_msvc", False):
    env.Append(CXXFLAGS=["/std:c++20"])
else:
    env.Append(CXXFLAGS=["-std=c++20"])
def linux_config():
    suffix = env['target']
    lib_filename = "{}{}.{}{}".format("lib", libname, suffix, env.subst('$SHLIBSUFFIX'))
    if env["platform"] == "linux":
        env.Append(LIBS=["libfmod%s" % logging, "libfmodstudio%s" % logging])
        env.Append(LINKFLAGS=["-Wl,-soname,{}.{}".format(lib_filename, so_number)])
def windows_config():
    if env["platform"] == "windows":
        env.Append(LIBS=["fmod%s_vc" % logging, "fmodstudio%s_vc" % logging])
def macos_config():
    if env["platform"] == "macos":
        env.Append(LIBS=['libfmod%s.dylib' % logging, 'libfmodstudio%s.dylib' % logging])
        env.Append(
            LINKFLAGS=[
                "-framework",
                "Cocoa",
                "-Wl,-undefined,dynamic_lookup",
                "-rpath", "@loader_path/.."
            ]
        )
def ios_config():
    if env["platform"] == "ios":
        env.Append(LIBS=['libfmod%s_iphoneos.a' % logging, 'libfmodstudio%s_iphoneos.a' % logging])
        env.Append(LINKFLAGS=[
            '-Wl,-undefined,dynamic_lookup', "-miphoneos-version-min=" + env["ios_min_version"]
        ])
def android_config():
    suffix = env['target']
    lib_filename = "{}{}.{}{}".format("lib", libname, suffix, env.subst('$SHLIBSUFFIX'))
    if env["platform"] == "android":
        env.Append(LIBS=['libfmod%s.so' % logging, 'libfmodstudio%s.so' % logging])
        env.Append(LINKFLAGS=["-Wl,-soname,{}.{}".format(lib_filename, so_number)])
def web_config():
    if env["platform"] == "web":
        if env["target"] == "editor":
            print_error("The editor target is not supported on web. Use target=template_debug or target=template_release.")
            Exit(1)
        if env["threads"]:
            print_error("Web builds only support threads=no.")
            Exit(1)
        # The Studio archive already contains the core library. Use File() so SCons links the
        # archive by path instead of turning it into -lfmodstudio*_wasm.
        fmod_lib = File("libs/web/wasm32/fmodstudio%s_wasm.a" % logging)
        if not os.path.isfile(fmod_lib.abspath):
            print_error("""%s not found.
    Run the following command to download the FMOD HTML5 libraries:

        python3 fmod_installer.py -t web setup <version>""" % fmod_lib.path)
            Exit(1)
        env.Append(LIBS=[fmod_lib])
def verify_godot_cpp():
    if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
        print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
    Run the following command to download godot-cpp:

        git submodule update --init --recursive""")
        sys.exit(1)
def gen_docs():
    if env["target"] in ["editor", "template_debug"]:
        try:
            doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
            sources.append(doc_data)
        except AttributeError:
            print("Not including class reference as we're targeting a pre-4.3 baseline.")
env.Append(CPPPATH=[env.Dir(d) for d in source_path])
# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(
    CPPPATH=[
        "src/",
        "src/scene/",
        "src/audio_streams/",
        "src/scene/2d/",
        "src/scene/3d",
        "src/binding/studio",
        "src/binding/",
        "headers/",
    ])
sources = [
    Glob("src/*.cpp"),
    Glob("src/scene/*.cpp"),
    Glob("src/audio_streams/*.cpp"),
    Glob("src/scene/2d/*.cpp"),
    Glob("src/scene/3d/*.cpp"),
    Glob("src/binding/*.cpp"),
    Glob("src/binding/studio/*.cpp"),
]
if env["target"] == "editor":
    env.Append(
    CPPPATH=[
        "src/editor/",
        "src/editor/plugins/",
        "src/editor/nodes/",
    ])
    sources.append([
        Glob("src/editor/*.cpp"),
        Glob("src/editor/plugins/*.cpp"),
        Glob("src/editor/nodes/*.cpp"),
    ])




verify_godot_cpp();
linux_config();
windows_config();
# Currently poorly supported by plugin Untested.
macos_config()
ios_config();
android_config();
web_config();


gen_docs();

env.Append(LIBPATH=["libs/%s/%s" % (env["platform"], env["arch"])])
suffix = env['target']
# Matches godot-cpp's suffix rule; threads is only disabled by default on web.
if not env["threads"]:
    suffix += ".nothreads"
lib_filename = "lib{}.{}{}".format(libname, suffix, env.subst('$SHLIBSUFFIX'))
library = env.SharedLibrary(
    "plugin_template/bin/{}/{}/{}".format(env["platform"], env["arch"], lib_filename),
    source=sources,
)
Default(library)
