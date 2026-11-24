"""Generate the extension's version header and package version."""

import argparse
import re
import subprocess
from pathlib import Path


def generate_version(source, output, version):
    if version:
        tag = ""
        unique_id = filename_version = version
    else:
        contents = (source / "product.version").read_text(encoding="utf-8")
        match = re.match(r"([0-9]+\.[0-9]+\.[0-9]+)(.*)", contents)
        if match is None:
            raise ValueError("Could not determine the product version.")
        version, tag = match.groups()
        rev = subprocess.check_output(
            ["git", "rev-list", "--count", "HEAD"], cwd=source, text=True
        ).strip()
        short_hash = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=source, text=True
        ).strip()
        unique_id = f"{rev}:{short_hash}"
        filename_version = f"{version}-git{rev}-{short_hash}"

    parts = version.split(".")
    parts += ["0"] * (4 - len(parts))
    file_version = ",".join(str(int(part)) for part in parts)
    (output / "version_auto.h").write_text(
        f'''#pragma once
#define SM_BUILD_TAG "{tag}"
#define SM_BUILD_UNIQUEID "{unique_id}" SM_BUILD_TAG
#define SM_VERSION "{version}"
#define SM_FULL_VERSION SM_VERSION SM_BUILD_TAG
#define SM_FILE_VERSION {file_version}
''',
        encoding="utf-8",
    )
    (output / "filename_versioning.txt").write_text(filename_version, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("version", nargs="?")
    args = parser.parse_args()
    generate_version(args.source, args.output, args.version)
