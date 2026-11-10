#!/usr/bin/env python3
"""Import Hi3516CV610 development files from a Buildroot SDK archive or host directory."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

SDK = Path(__file__).resolve().parent.parent
GROUPS = ("common", "mpp", "isp", "svp")


def group(name, library=False):
    stem = name.removeprefix("lib") if library else name
    if re.search(r"svp|protobuf|ive|ivs|(^|_)md\b|face_capture|(^|_)fr\b|vlpr|aibnr|ai3dnr|aidetect|aivsr|aiseg|aidrc|aiservice|aiisp", stem):
        return "svp"
    if re.search(r"isp|(^|_)ae\b|(^|_)awb\b|(^|_)af\b|sns|sensor|smartae|piris|^(acs|bnr|calcflicker|dehaze|drc|extend_stats|ir_auto|ldci)$", stem):
        return "isp"
    if re.search(r"^(ot_type|ot_common|ot_defines|ot_errno|ot_debug|ot_math|autoconf|securec|securectype|list|irq_common|mmz|osal.*|ot_osal.*)\.(h|so|a)$", name):
        return "common"
    if library and stem in {"securec", "ot_osal", "ot_base", "ot_irq", "ot_sys", "ot_vb", "ot_pm", "ot_chnl", "ot_vca", "ss_mpi", "ss_mpi_sysbind", "ss_mpi_sysmem", "ot_mpi_sysmem"}:
        return "common"
    if not library and re.search(r"(^|_)(sys|vb|sys_mem|sys_bind|syskol)(\.|_)|^osal", stem):
        return "common"
    return "mpp"


def copy_entry(src, dst):
    if src.is_dir():
        shutil.copytree(src, dst, symlinks=True)
    elif src.is_symlink():
        target = src.readlink()
        if target.is_absolute():
            raise RuntimeError(f"absolute symlink in platform input: {src} -> {target}")
        dst.symlink_to(target)
    else:
        shutil.copy2(src, dst)


def is_development_file(path):
    """Select development artifacts without changing their installed paths."""
    name = path.name
    upper = name.upper()
    if upper.startswith(("LICENSE", "COPYING", "COPYRIGHT", "NOTICE")):
        return True
    if any(part.lower() in {"license", "licenses"} for part in path.parts):
        return True
    # Includes may contain extensionless headers and generated definitions.
    if "include" in path.parts:
        return True
    if path.suffix.lower() in {".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".tcc",
                               ".a", ".pc", ".cmake", ".m4", ".gir", ".vapi", ".deps"}:
        return True
    # Keep both link names and versioned library files, but not helpers such
    # as libfoo.so-gdb.py. Obsolete .la files contain build-machine paths.
    return re.search(r"\.so(?:\.[0-9]+)*$", name) is not None


def export_packages(host, dest, package_manifest):
    """Use Buildroot ownership records, not guessed include-directory names."""
    if package_manifest is None:
        candidates = [host.parent / "build/packages-file-list-staging.txt",
                      host / "share/v610-platform/packages-file-list-staging.txt"]
        package_manifest = next((p for p in candidates if p.is_file()), None)
    if package_manifest is None or not package_manifest.is_file():
        raise RuntimeError("Buildroot package ownership manifest missing; use output/host, "
                           "or pass --package-manifest <output/build/packages-file-list-staging.txt>")
    sysroot = host / "arm-buildroot-linux-musleabi/sysroot"
    packages = {}
    for line in package_manifest.read_text().splitlines():
        package, sep, rel = line.partition(",")
        if not sep or package.startswith(("host-", "toolchain", "skeleton")):
            continue
        # These are managed by their SDK components or by the vendor group.
        if package in {"hisi-osdrv", "libdrm", "opencv", "opencv4", "lvgl"}:
            continue
        rel = Path(rel.removeprefix("./"))
        if rel.is_absolute() or ".." in rel.parts:
            raise RuntimeError(f"invalid package path: {rel}")
        if not is_development_file(rel):
            continue
        source = sysroot / rel
        if not source.exists() and not source.is_symlink():
            raise RuntimeError(f"package file missing from sysroot: {package}: {rel}")
        if source.is_dir():
            continue
        relative = rel
        target = dest / "packages" / package / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() and not target.is_symlink():
            copy_entry(source, target)
        if target.suffix == ".pc":
            text = target.read_text()
            # /usr paths now refer to the package's usr directory; runtime paths such as
            # /etc and D-Bus socket addresses remain target runtime paths.
            text = re.sub(r"/usr(?=/|\s|$)", "${pcfiledir}/../..", text)
            text = re.sub(r"^prefix=.*$", "prefix=${pcfiledir}/../..", text, flags=re.M)
            # Older pkg-config expands variables before splitting flags. Quote
            # include/library operands so relocated SDK paths may contain spaces.
            text = re.sub(r"(?<!\S)(-[IL])([^\s\"']+)", r'\1"\2"', text)
            target.write_text(text)
        packages.setdefault(package, {"files": [], "pkg_config": []})["files"].append(str(relative))
        if target.suffix == ".pc":
            packages[package]["pkg_config"].append(target.stem)
    for package, info in packages.items():
        info["file_count"] = len(set(info.pop("files")))
        info["pkg_config"] = sorted(set(info["pkg_config"]))
    return dict(sorted(packages.items()))


def hisilicon_source(host):
    metadata = host / "share/v610-platform/hisilicon-source.json"
    if metadata.is_file():
        return json.loads(metadata.read_text())
    # Compatibility with existing local builds made before provenance was exported.
    builds = sorted((host.parent / "build").glob("hisi-osdrv-*"))
    if len(builds) == 1:
        version = builds[0].name.removeprefix("hisi-osdrv-")
        return {"sdk": "Hi3516CV610", "version": version,
                "buildroot_package": "hisi-osdrv",
                "identified_by": "build-directory"}
    return {"sdk": "Hi3516CV610", "version": None,
            "buildroot_package": "hisi-osdrv"}


def export(host, dest, source_id, package_manifest):
    sysroot = host / "arm-buildroot-linux-musleabi/sysroot"
    headers = sysroot / "usr/include/hisi"
    libraries = sysroot / "usr/lib"
    if not (headers / "ot_type.h").is_file():
        raise RuntimeError(f"missing HiSilicon headers in {host}")
    for g in GROUPS:
        for sub in ("include", "lib"):
            (dest / "hisilicon" / g / sub).mkdir(parents=True)
    licenses = dest / "hisilicon/licenses"
    licenses.mkdir()
    for src in sorted(headers.iterdir()):
        if src.name.upper().startswith(("LICENSE", "COPYING", "NOTICE")):
            copy_entry(src, licenses / src.name)
        else:
            copy_entry(src, dest / "hisilicon" / group(src.name) / "include" / src.name)
    inventory = json.loads((SDK / "scripts/platform-libraries.json").read_text())
    count = 0
    for name in inventory:
        candidates = list(libraries.glob(name + ".so*")) + list(libraries.glob(name + ".a"))
        for src in sorted(candidates):
            copy_entry(src, dest / "hisilicon" / group(name[3:], True) / "lib" / src.name)
            count += 1
    if not count:
        raise RuntimeError("no platform libraries found")
    for required in ("libss_mpi.so", "libsvp_acl.so", "libsns_gc4023.a"):
        if not list(dest.glob(f"hisilicon/*/lib/{required}")):
            raise RuntimeError(f"missing required platform library: {required}")
    compiler = host / "opt/ext-toolchain/arm-v01c02-linux-musleabi-gcc/bin/arm-v01c02-linux-musleabi-gcc"
    version = subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0]
    packages = export_packages(host, dest, package_manifest)
    manifest = {
        "schema_version": 5,
        "platform_version": (SDK / "VERSION").read_text().strip(),
        "target": {
            "chip": "hi3516cv610",
            "board": "cove_610",
            "abi": {"cpu": "cortex-a7", "float": "softfp", "fpu": "neon-vfpv4", "libc": "musl"},
        },
        "compiler": {
            "version": subprocess.check_output([str(compiler), "-dumpfullversion"], text=True).strip(),
            "target": subprocess.check_output([str(compiler), "-dumpmachine"], text=True).strip(),
            "description": version,
        },
        "source": source_id,
        "hisilicon": {
            "source": hisilicon_source(host),
            "libraries": {g: sorted(p.name for p in (dest / "hisilicon" / g / "lib").iterdir()) for g in GROUPS},
        },
        "packages": {
            "export_policy": "development-files-original-layout",
            "items": packages,
        },
    }
    content = hashlib.sha256()
    for p in sorted(dest.rglob("*")):
        if p.is_file():
            content.update(str(p.relative_to(dest)).encode())
            content.update(p.read_bytes())
    manifest["content_sha256"] = content.hexdigest()
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for p in dest.rglob("*"):
        if not p.is_symlink():
            if p.is_dir():
                p.chmod(0o755)
    dest.chmod(0o755)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Buildroot SDK archive or output/host directory")
    parser.add_argument("--package-manifest", type=Path, help="Buildroot staging package ownership list (for older archives)")
    args = parser.parse_args()
    source = args.input.resolve()
    if not source.exists():
        parser.error(f"input does not exist: {source}")
    with tempfile.TemporaryDirectory(prefix=".platform.", dir=SDK) as tmp:
        work = Path(tmp)
        if source.is_dir():
            host = source
            # Hash the actual imported files so the manifest identifies the platform content.
            digest = hashlib.sha256()
            root = host / "arm-buildroot-linux-musleabi/sysroot/usr"
            for p in sorted((root / "include/hisi").rglob("*")):
                if p.is_file():
                    digest.update(str(p.relative_to(root)).encode())
                    digest.update(p.read_bytes())
            for name in json.loads((SDK / "scripts/platform-libraries.json").read_text()):
                for p in sorted((root / "lib").glob(name + ".*")):
                    if p.is_file():
                        digest.update(p.name.encode())
                        digest.update(p.read_bytes())
            source_id = {"kind": "buildroot-host", "sha256": digest.hexdigest()}
        else:
            digest = hashlib.sha256()
            with source.open("rb") as f:
                for block in iter(lambda: f.read(1024 * 1024), b""):
                    digest.update(block)
            source_id = {"kind": "buildroot-sdk", "archive": source.name, "sha256": digest.hexdigest()}
            # tar preserves the original relative symlinks and executable permissions.
            with tarfile.open(source) as archive:
                top = archive.next().name.split("/")[0]
            subprocess.run(["tar", "-xf", str(source), "-C", str(work)], check=True)
            host = work / top
        dest = work / "new-platform/hi3516cv610"
        export(host, dest, source_id, args.package_manifest)
        old = SDK / "platform"
        # Validate all generated links before replacing the existing platform.
        for p in dest.rglob("*"):
            if p.is_symlink() and (p.readlink().is_absolute() or not p.exists()):
                raise RuntimeError(f"invalid platform symlink: {p}")
        backup = work / "old-platform"
        if old.exists():
            old.rename(backup)
        try:
            (work / "new-platform").rename(old)
        except BaseException:
            if backup.exists():
                backup.rename(old)
            raise
        print(f"platform installed: {old / 'hi3516cv610'}")


if __name__ == "__main__":
    main()
