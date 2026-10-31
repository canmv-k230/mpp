#!/usr/bin/env python3
"""Configure an SDK output without modifying FFmpeg's checked-in K230 preset."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    output.mkdir(parents=True, exist_ok=True)
    destination = output / ".sdk-config"
    with tempfile.TemporaryDirectory(prefix=".configure-", dir=output) as temporary:
        staged = Path(temporary)
        # FFmpeg supports a src/ link for an out-of-tree configuration. Its
        # generated headers and component lists stay entirely in this directory.
        (staged / "src").symlink_to(source, target_is_directory=True)
        status = subprocess.call([str(source / "configure"), *arguments], cwd=staged)
        if status:
            log = staged / "ffbuild/config.log"
            if log.is_file():
                (output / "ffbuild").mkdir(exist_ok=True)
                log.replace(output / "ffbuild/config-failed.log")
            return status
        for name in ("config.h", "ffbuild/config.mak", "ffbuild/config.sh",
                     "ffbuild/.config", "libavutil/avconfig.h"):
            if not (staged / name).is_file():
                raise ValueError("configure did not generate {}".format(name))
        with tempfile.TemporaryDirectory(prefix=".configure-backup-", dir=output) as backup:
            saved = Path(backup) / "previous"
            if destination.exists():
                destination.rename(saved)
            try:
                staged.rename(destination)
            except BaseException:
                if saved.exists():
                    saved.rename(destination)
                raise
    print("FFmpeg configuration saved in {}".format(destination))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit("FFmpeg configuration: {}".format(error))
