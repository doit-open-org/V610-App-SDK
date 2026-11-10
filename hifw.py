#!/usr/bin/env python3

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


HIFW_PATH = Path(__file__).resolve().parent
# kconfiglib ships with the SDK so that no Python package has to be installed.
sys.path.insert(0, str(HIFW_PATH / "tools" / "kconfiglib"))
BUILD_CONFIG_NAME = "hifw.conf"
CHIP_TOOLCHAINS = {
    "hi3516cv610": "hi3516cv610",
}


def read_config(config_path):
    if not config_path.is_file():
        raise RuntimeError(f"configuration not found: {config_path}")

    values = {}
    for line_number, raw_line in enumerate(
        config_path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise RuntimeError(
                f"{config_path}:{line_number}: expected KEY=VALUE"
            )
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        if not re.fullmatch(r"[A-Z][A-Z0-9_]*", key):
            raise RuntimeError(
                f"{config_path}:{line_number}: invalid key '{key}'"
            )
        values[key] = value
    return values


def project_directory(args):
    project_dir = Path(args.project_dir).resolve()
    if not (project_dir / "CMakeLists.txt").is_file():
        raise RuntimeError(f"not a HiFW project: {project_dir}")
    return project_dir


def build_config_path(project_dir):
    return project_dir / "build" / BUILD_CONFIG_NAME


def selected_settings(args):
    project_dir = project_directory(args)
    config_path = build_config_path(project_dir)
    if not config_path.is_file():
        raise RuntimeError(
            "target is not configured; run "
            f"'hifw.py -C {project_dir} set-target' first"
        )

    config = read_config(config_path)
    chip = config.get("CHIP", "")
    if not chip or not re.fullmatch(r"[A-Za-z0-9_.-]+", chip):
        raise RuntimeError(f"CHIP must be set to a valid name in {config_path}")

    toolchain_file = toolchain_file_for_chip(chip)
    if toolchain_file is None:
        available = ", ".join(available_chips()) or "none"
        raise RuntimeError(
            f"unsupported CHIP '{chip}'; available chips: {available}"
        )

    build_type = getattr(args, "build_type", None) or config.get(
        "BUILD_TYPE", "Release"
    )
    output_setting = config.get("OUTPUT_DIR", "out")
    output_dir = (project_dir / output_setting).resolve()

    build_key_source = f"{chip}-{build_type}"
    build_key = re.sub(r"[^A-Za-z0-9_.-]", "_", build_key_source)
    build_dir = project_dir / "build" / build_key

    return {
        "project_dir": project_dir,
        "config": config,
        "chip": chip,
        "toolchain_file": toolchain_file.resolve(),
        "build_type": build_type,
        "output_dir": output_dir,
        "build_dir": build_dir,
    }


def set_target(args):
    project_dir = project_directory(args)
    chip = args.chip
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", chip):
        raise RuntimeError(f"invalid chip name: {chip}")

    toolchain_file = toolchain_file_for_chip(chip)
    if toolchain_file is None:
        available = ", ".join(available_chips()) or "none"
        raise RuntimeError(
            f"unsupported chip '{chip}'; available chips: {available}"
        )

    build_root = project_dir / "build"
    shutil.rmtree(build_root, ignore_errors=True)
    build_root.mkdir(parents=True, exist_ok=True)

    values = [
        ("CHIP", chip),
        ("BUILD_TYPE", args.build_type),
        ("OUTPUT_DIR", args.output_dir),
    ]

    config_path = build_config_path(project_dir)
    config_path.write_text(
        "".join(f"{key}={value}\n" for key, value in values),
        encoding="utf-8",
    )
    print(f"[hifw] target set to {chip}")
    print(f"[hifw] config: {config_path}")


def available_chips():
    return sorted(
        chip
        for chip in CHIP_TOOLCHAINS
        if toolchain_file_for_chip(chip) is not None
        and (HIFW_PATH / "cmake" / "chips" / f"{chip}.cmake").is_file()
    )


def embedded_vendor_sdk():
    """The vendor SDK of the source tree this SDK is embedded in, if any.

    A vendor solution checkout carries this SDK as
    apps/<board>/V610-App-SDK and keeps its built vendor SDK in
    _build/sdk/Hi3516CV610. The path is taken as given (not resolved), so
    that it also works when the SDK directory is a link.
    """
    roots = [Path(os.path.abspath(__file__)).parent]
    for name in ("HIFW_PATH", "APP_SDK_PATH"):
        if os.environ.get(name):
            roots.insert(0, Path(os.path.abspath(os.environ[name])))
    for root in roots:
        if len(root.parents) < 3 or root.parents[1].name != "apps":
            continue
        sdk = root.parents[2] / "_build" / "sdk" / "Hi3516CV610"
        if (sdk / "soc" / "include").is_dir():
            return sdk
    return None


def export_sdk_path():
    """Tell the project where this SDK is.

    Projects read APP_SDK_PATH; HIFW_PATH is the name older projects use.
    Both are set by export.sh, and defaulted here so that a project also
    builds when hifw.py is called by its path without sourcing it.
    """
    here = os.path.abspath(os.path.dirname(__file__))
    sdk_path = os.environ.get("APP_SDK_PATH") or os.environ.get("HIFW_PATH") or here
    os.environ["APP_SDK_PATH"] = sdk_path
    os.environ.setdefault("HIFW_PATH", sdk_path)


def select_platform():
    """Choose between the two platforms the same way export.sh does.

    buildroot: the platform files generated with the Buildroot board image.
    solution:  the built vendor SDK of a hi_aiot_solution checkout, named
               by HI_SDK_ROOT or found around this SDK.
    APP_SDK_PLATFORM, exported by export.sh, settles it; without it the
    choice follows HI_SDK_ROOT and the location of this SDK.
    """
    platform = os.environ.get("APP_SDK_PLATFORM", "")
    if platform not in ("", "buildroot", "solution"):
        raise RuntimeError(
            f"APP_SDK_PLATFORM must be 'buildroot' or 'solution', not '{platform}'"
        )
    if platform == "buildroot":
        os.environ.pop("HI_SDK_ROOT", None)
        return
    if not os.environ.get("HI_SDK_ROOT"):
        sdk = embedded_vendor_sdk()
        if sdk is not None:
            os.environ["HI_SDK_ROOT"] = str(sdk)
    if os.environ.get("HI_SDK_ROOT"):
        os.environ.pop("HIFW_TOOLCHAIN_PATH", None)
    elif platform == "solution":
        raise RuntimeError(
            "APP_SDK_PLATFORM=solution needs a built vendor SDK; set HI_SDK_ROOT"
        )


def toolchain_file_for_chip(chip):
    toolchain = CHIP_TOOLCHAINS.get(chip)
    if toolchain is None:
        return None
    # A vendor SDK tree named by HI_SDK_ROOT replaces the platform files.
    if os.environ.get("HI_SDK_ROOT"):
        toolchain = "vendor_sdk"
    path = HIFW_PATH / "cmake" / "toolchains" / f"{toolchain}.cmake"
    return path if path.is_file() else None


def config_source(project_dir):
    active = project_dir / ".config"
    if active.is_file():
        return active
    defaults = project_dir / "sdkconfig.defaults"
    if defaults.is_file():
        return defaults
    return None


def component_directories(project_dir, include_main=True):
    components = {}
    for root in (HIFW_PATH / "components", project_dir / "components"):
        if not root.is_dir():
            continue
        for component_dir in sorted(root.iterdir()):
            if (component_dir / "CMakeLists.txt").is_file():
                components[component_dir.name] = component_dir

    if include_main:
        main_dir = project_dir / "main"
        if (main_dir / "CMakeLists.txt").is_file():
            components["main"] = main_dir
    return [components[name] for name in sorted(components)]


def kconfig_source_line(source):
    escaped_source = str(source).replace("\\", "\\\\").replace('"', '\\"')
    return f'source "{escaped_source}"\n'


def generate_kconfig_root(project_dir):
    application_sources = []
    for source in (project_dir / "Kconfig", project_dir / "main" / "Kconfig"):
        if source.is_file():
            application_sources.append(source)

    component_sources = [
        component_dir / "Kconfig"
        for component_dir in component_directories(project_dir, include_main=False)
        if (component_dir / "Kconfig").is_file()
    ]

    destination = project_dir / "build" / "Kconfig"
    destination.parent.mkdir(parents=True, exist_ok=True)
    content = 'mainmenu "HiFW Configuration"\n\n'
    if application_sources:
        content += 'menu "Application Modules"\n'
        content += "".join(kconfig_source_line(source) for source in application_sources)
        content += "endmenu\n\n"
    if component_sources:
        content += 'menu "Component Modules"\n'
        content += "".join(kconfig_source_line(source) for source in component_sources)
        content += "endmenu\n"
    if not destination.exists() or destination.read_text(encoding="utf-8") != content:
        destination.write_text(content, encoding="utf-8")
    return destination


def load_kconfig(project_dir):
    try:
        import kconfiglib
    except ImportError as error:
        raise RuntimeError(
            "configuration requires the Python kconfiglib package"
        ) from error

    kconfig_path = generate_kconfig_root(project_dir)
    kconf = kconfiglib.Kconfig(str(kconfig_path), warn=True)
    source = config_source(project_dir)
    if source:
        kconf.load_config(str(source), replace=True)
    return kconf


def generate_config_header(project_dir, destination, kconf=None):
    if kconf is None:
        kconf = load_kconfig(project_dir)
    destination.parent.mkdir(parents=True, exist_ok=True)

    temporary = destination.with_suffix(f"{destination.suffix}.tmp")
    kconf.write_autoconf(str(temporary), header="")
    content = temporary.read_text(encoding="utf-8")
    temporary.unlink()
    if not destination.exists() or destination.read_text(encoding="utf-8") != content:
        destination.write_text(content, encoding="utf-8")


def enabled_components(project_dir, kconf=None):
    if kconf is None:
        kconf = load_kconfig(project_dir)

    enabled = []
    for component_dir in component_directories(project_dir, include_main=False):
        component_name = component_dir.name
        if not (component_dir / "Kconfig").is_file():
            enabled.append(component_name)
            continue
        symbol_name = component_name.upper()
        symbol = kconf.syms.get(symbol_name)
        if symbol is None:
            enabled.append(component_name)
        elif symbol.str_value == "y":
            enabled.append(component_name)
    return enabled


def cache_value(cache_path, variable):
    if not cache_path.is_file():
        return None
    prefix = f"{variable}:"
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1]
    return None


def configure_and_build(args, fresh=False):
    settings = selected_settings(args)
    build_dir = settings["build_dir"]
    cache_path = build_dir / "CMakeCache.txt"
    expected_toolchain = str(settings["toolchain_file"])
    cached_toolchain = cache_value(cache_path, "CMAKE_TOOLCHAIN_FILE")
    if fresh or (cached_toolchain and cached_toolchain != expected_toolchain):
        shutil.rmtree(build_dir, ignore_errors=True)

    config_header = build_dir / "generated" / "hifw_config.h"
    kconf = load_kconfig(settings["project_dir"])
    generate_config_header(settings["project_dir"], config_header, kconf)
    components = enabled_components(settings["project_dir"], kconf)

    configure_command = [
        "cmake",
        "-S",
        str(settings["project_dir"]),
        "-B",
        str(build_dir),
        f"-DCMAKE_TOOLCHAIN_FILE={settings['toolchain_file']}",
        f"-DCMAKE_BUILD_TYPE={settings['build_type']}",
        f"-DHIFW_CHIP={settings['chip']}",
        f"-DHIFW_CONFIG_HEADER={config_header}",
        f"-DHIFW_OUTPUT_DIR={settings['output_dir']}",
        f"-DHIFW_COMPONENTS={';'.join(components)}",
    ]
    for name in ("HIFW_PLATFORM_PATH", "HI_SDK_ROOT", "CROSS_PREFIX"):
        value = os.environ.get(name, "")
        if value:
            configure_command.append(f"-D{name}={value}")
    subprocess.run(configure_command, check=True)

    jobs = args.jobs or os.cpu_count() or 1
    subprocess.run(
        ["cmake", "--build", str(build_dir), "-j", str(jobs)], check=True
    )
    print(f"[hifw] chip: {settings['chip']}")
    print(f"[hifw] build: {build_dir}")
    print(f"[hifw] output: {settings['output_dir']}")


def run_menuconfig(args):
    settings = selected_settings(args)
    project_dir = settings["project_dir"]

    try:
        import menuconfig
    except ImportError as error:
        raise RuntimeError(
            "menuconfig requires the Python kconfiglib package"
        ) from error

    previous_cwd = Path.cwd()
    previous_config = os.environ.get("KCONFIG_CONFIG")
    try:
        os.chdir(project_dir)
        os.environ["KCONFIG_CONFIG"] = str(project_dir / ".config")
        kconf = load_kconfig(project_dir)
        active_config = project_dir / ".config"
        menuconfig.menuconfig(kconf)
        kconf.write_config(str(active_config))
    finally:
        os.chdir(previous_cwd)
        if previous_config is None:
            os.environ.pop("KCONFIG_CONFIG", None)
        else:
            os.environ["KCONFIG_CONFIG"] = previous_config


def clean_project(args, full=False):
    settings = selected_settings(args)
    if full:
        shutil.rmtree(settings["project_dir"] / "build", ignore_errors=True)
        shutil.rmtree(settings["output_dir"], ignore_errors=True)
        print("[hifw] removed all build and output files")
        return

    build_dir = settings["build_dir"]
    if (build_dir / "CMakeCache.txt").is_file():
        subprocess.run(
            ["cmake", "--build", str(build_dir), "--target", "clean"],
            check=True,
        )
    else:
        print(f"[hifw] build directory does not exist: {build_dir}")


def install_project(args):
    settings = selected_settings(args)
    build_dir = settings["build_dir"]
    if not (build_dir / "CMakeCache.txt").is_file():
        raise RuntimeError("project is not built; run 'hifw.py build' first")

    command = ["cmake", "--install", str(build_dir), "--prefix", args.prefix]
    if args.strip:
        command.append("--strip")
    environment = dict(os.environ)
    if args.destdir:
        environment["DESTDIR"] = str(Path(args.destdir).resolve())
    subprocess.run(command, check=True, env=environment)


def create_parser():
    parser = argparse.ArgumentParser(prog="hifw.py")
    parser.add_argument(
        "-C", "--project-dir", default=".", help="HiFW project directory"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    target_parser = subparsers.add_parser("set-target")
    target_parser.add_argument(
        "chip", nargs="?", default="hi3516cv610", help="chip toolchain name"
    )
    target_parser.add_argument("--build-type", default="Release")
    target_parser.add_argument("--output-dir", default="out")

    for command in ("build", "reconfigure"):
        command_parser = subparsers.add_parser(command)
        command_parser.add_argument("--build-type", default=None)
        command_parser.add_argument("-j", "--jobs", type=int, default=None)

    install_parser = subparsers.add_parser("install")
    install_parser.add_argument(
        "--destdir", default="", help="staging root the prefix is placed under"
    )
    install_parser.add_argument("--prefix", default="/usr")
    install_parser.add_argument("--strip", action="store_true")

    subparsers.add_parser("menuconfig")
    subparsers.add_parser("clean")
    subparsers.add_parser("fullclean")
    subparsers.add_parser("list-chips")
    return parser


def main():
    parser = create_parser()
    args = parser.parse_args()
    try:
        export_sdk_path()
        select_platform()
        if args.command == "set-target":
            set_target(args)
        elif args.command == "build":
            configure_and_build(args)
        elif args.command == "reconfigure":
            configure_and_build(args, fresh=True)
        elif args.command == "install":
            install_project(args)
        elif args.command == "menuconfig":
            run_menuconfig(args)
        elif args.command == "clean":
            clean_project(args)
        elif args.command == "fullclean":
            clean_project(args, full=True)
        elif args.command == "list-chips":
            for chip in available_chips():
                print(chip)
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"[hifw] ERROR: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
