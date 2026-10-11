#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
import platform
from pathlib import Path
from fmod_installer import get_os

supported_platforms = ["linux", "windows"]

script_dir = os.path.dirname(os.path.abspath(__file__))


def get_arch() -> str:
    arch = platform.machine()
    if arch == "AMD64":
        arch = "x86_64"
    return arch


def resolveArch(platform, arch):
    if arch:
        return arch
    if platform == "web":
        return "wasm32"
    return get_arch()


def runScons(platform, arch, target, build_profile, extra_args):
    p = subprocess.run(
        [
            "scons",
            "use_static_cpp=yes",
            "debug_symbols=no",
            *extra_args,
            "build_profile=%s" % build_profile,
            "target=%s" % target,
            "platform=%s" % platform,
            "arch=%s" % arch,
            "api_version=4.6",
        ]
    )
    return p.returncode == 0


def buildForPlatform(platform, arch, target:str):
    target = target.lower()
    extra_args = ["lto=auto"]
    if platform == "web":
        # FMOD's HTML5 libraries are linked statically into a single-threaded side module.
        # lto=auto would become lto=full on web; lto=none matches CI and the tested setup.
        extra_args = ["lto=none", "threads=no"]
        if target == "editor":
            print("the editor target is not supported on web")
            exit(1)
    if(target in ["release", "all"]):
        if not runScons(platform, arch, "template_release", "build_profile.json", extra_args):
            print("failed to build for release")
            exit(1)
    if(target in ["debug", "all"]):
        if not runScons(platform, arch, "template_debug", "build_profile.json", extra_args):
            print("failed to build for debug")
            exit(1)

    if(target in ["editor", "all"]):
        if platform == "web":
            print("skipping editor build: the editor target is not supported on web")
        elif not runScons(platform, arch, "editor", "build_profile_editor.json", extra_args):
            print("failed to build for editor")
            exit(1)


def exportDependencies(platform, arch):
    os.makedirs(
        os.path.join(script_dir, "plugin_template", "bin", platform, arch),
        exist_ok=True,
    )
    # FMOD is linked statically on web, so there are no libraries to ship.
    if platform != "web":
        for file in os.listdir(os.path.join(script_dir, "libs", platform, arch)):
            shutil.copy(
                os.path.join(script_dir, "libs", platform, arch, file),
                os.path.join(script_dir, "plugin_template", "bin", platform, arch, file),
            )
    shutil.copy(
        os.path.join(script_dir, "LICENSE.txt"),
        os.path.join(script_dir, "plugin_template", "LICENSE.txt"),
    )



def main():
    parser = argparse.ArgumentParser(
        prog="ExportFmodGodot",
        description="builds FmodGodot gdextension for platform and can export addon to a folder",
    )
    subparsers = parser.add_subparsers(
        dest="command",
        help="Descriptions",
        required=True,
        description="Subcommand to run",
    )
    build = subparsers.add_parser(
        name="build",
        help="Build Plugin",
        description="Builds the plugins and places it in plugin_templates",
    )
    export = subparsers.add_parser(
        name="export",
        help="Export plugin",
        description="Copies the contents of plugin_template to destination. Likely <path>/<to>/<project>/addons",
    )
    push = subparsers.add_parser(
        name="push",
        help="Push Dependencies",
        description="Copies necessary files ie the fmod libs to plugin_template/",
    )
    export.add_argument("destination", type=Path, help="the destination folder")
    export.add_argument(
        "-f", "--force", help="Overwrite files in destination", action="store_true"
    )
    build.add_argument(
        "-p", "--platform", type=str, help="the platform to build for", default=get_os()
    )
    build.add_argument(
        "-t", "--target", type=str, help="The target template to build", default="all"
    )
    build.add_argument(
        "-a",
        "--architecture",
        type=str,
        help="the cpu architecture to build for (default: wasm32 for web, the host architecture otherwise)",
        default=None,
    )

    push.add_argument(
        "-p", "--platform", type=str, help="the platform to build for", default=get_os()
    )
    push.add_argument(
        "-a",
        "--architecture",
        type=str,
        help="the cpu architecture to build for (default: wasm32 for web, the host architecture otherwise)",
        default=None,
    )
    args = parser.parse_args()

    match args.command:
        case "build":
            if args.platform == "all":
                for os_platform in supported_platforms:
                    arch = resolveArch(os_platform, args.architecture)
                    buildForPlatform(os_platform, arch, args.target)
                    exportDependencies(os_platform, arch)
            else:
                arch = resolveArch(args.platform, args.architecture)
                buildForPlatform(args.platform, arch, args.target)
                exportDependencies(args.platform, arch)
        case "export":
            if args.destination == None or not args.destination.is_dir():
                print(
                    '"'
                    + str(args.destination)
                    + '" does not exist or is not a directory'
                )
                return
            path = Path(args.destination)
            pathA = Path(str(path).removesuffix('"')).joinpath("FmodGodot")
            force = args.force
            if args.force == None:
                force = False
            if pathA.exists():
                if not force:
                    print(str(pathA) + " already exists overrwrite ? (y/n)")
                    input_string = input()
                    if input_string.lower() == "y\n" or input_string.lower() == "yes\n":
                        shutil.copytree("plugin_template", pathA, dirs_exist_ok=True)
                    else:
                        return
                shutil.copytree("plugin_template", pathA, dirs_exist_ok=True)
            else:
                shutil.copytree("plugin_template", pathA)
        case "push":
            if args.platform == "all":
                for os_platform in supported_platforms:
                    exportDependencies(os_platform, resolveArch(os_platform, args.architecture))
            else:
                exportDependencies(args.platform, resolveArch(args.platform, args.architecture))
        case _:
            exit(1)


if __name__ == "__main__":
    main()
