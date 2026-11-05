"""Usage: python3 -I build_zip.py [--check] ALTTPR PACKAGE_DIR DATA_DIR ZIP

Writes ZIP, the release whose only top-level folder is App/ALttPR/: ALTTPR (the device binary),
launch.sh, config.json and icon.png from PACKAGE_DIR, base.bps, DejaVuSansMono.ttf and
DejaVuSansMono-LICENSE.txt from DATA_DIR. The same inputs give the same bytes. After writing, reads
ZIP back and checks its entry list, every file's bytes against its source and the modes.
--check only reads an existing ZIP back against the inputs. Exits non-zero with one line on error.
"""

import os
import sys
import zipfile

ROOT = "App/ALttPR/"
EPOCH = (1980, 1, 1, 0, 0, 0)
UNIX = 3
DIR_MODE = 0o40755
EXEC_MODE = 0o100755
FILE_MODE = 0o100644
MSDOS_DIRECTORY = 0x10


class PackageError(Exception):
    pass


def sources(binary, package_dir, data_dir):
    """{entry name: (source path, mode)} for every file entry."""
    files = {"alttpr": (binary, EXEC_MODE)}
    for name, mode in (("launch.sh", EXEC_MODE), ("config.json", FILE_MODE), ("icon.png", FILE_MODE)):
        files[name] = (os.path.join(package_dir, name), mode)
    for name in ("base.bps", "DejaVuSansMono.ttf", "DejaVuSansMono-LICENSE.txt"):
        files[name] = (os.path.join(data_dir, name), FILE_MODE)
    return {ROOT + name: src for name, src in files.items()}


def expected_entries(files):
    return sorted(["App/", ROOT] + list(files))


def entry(name, mode):
    info = zipfile.ZipInfo(name, date_time=EPOCH)
    info.create_system = UNIX
    info.external_attr = mode << 16
    if name.endswith("/"):
        info.external_attr |= MSDOS_DIRECTORY
    else:
        info.compress_type = zipfile.ZIP_DEFLATED
    return info


def read_inputs(files):
    contents = {}
    for name, (path, _) in files.items():
        try:
            with open(path, "rb") as f:
                contents[name] = f.read()
        except OSError:
            raise PackageError("missing input: %s" % path)
    return contents


def write_zip(path, files, contents):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with zipfile.ZipFile(path, "w") as z:
        for name in expected_entries(files):
            if name.endswith("/"):
                z.writestr(entry(name, DIR_MODE), b"")
            else:
                z.writestr(entry(name, files[name][1]), contents[name], compresslevel=9)


def check_zip(path, files, contents):
    try:
        z = zipfile.ZipFile(path)
    except (OSError, zipfile.BadZipFile) as e:
        raise PackageError("unreadable zip %s: %s" % (path, e))
    with z:
        names = z.namelist()
        if names != expected_entries(files):
            raise PackageError("%s: entries %s, expected %s" % (path, names, expected_entries(files)))
        for info in z.infolist():
            mode = DIR_MODE if info.is_dir() else files[info.filename][1]
            if info.external_attr >> 16 != mode:
                raise PackageError("%s: %s has mode %o, expected %o"
                                   % (path, info.filename, info.external_attr >> 16, mode))
            if not info.is_dir() and z.read(info) != contents[info.filename]:
                raise PackageError("%s: %s differs from %s" % (path, info.filename, files[info.filename][0]))


def main(argv):
    args = argv[1:]
    check_only = args[:1] == ["--check"]
    if check_only:
        args = args[1:]
    if len(args) != 4:
        sys.exit(__doc__.strip())
    binary, package_dir, data_dir, out = args
    files = sources(binary, package_dir, data_dir)
    try:
        contents = read_inputs(files)
        if not check_only:
            write_zip(out, files, contents)
        check_zip(out, files, contents)
    except PackageError as e:
        sys.exit("build_zip: %s" % e)


if __name__ == "__main__":
    main(sys.argv)
