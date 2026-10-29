"""Run host sanitizer checks without building or linking the hardware SDK.

Usage: python3 tests/run_packet_bounds.py
The generated includes use production functions verbatim; only external boundaries
are stubbed in packet_bounds.c. Requires a C compiler supporting AddressSanitizer.
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    source = (root / "neuralert/src/apps/neuralert.c").read_text()
    names = ("append_packet_field", "send_json_packet", "neuralert_provisioning_step1")
    functions = []
    for name in names:
        start = re.search(rf"^(?:static )?int {name}\(", source, re.MULTILINE)
        if start is None:
            raise RuntimeError(f"Production function {name} not found")
        end = source.index("\n}\n", start.start()) + 3
        functions.append(source[start.start():end])
    constants = []
    for name in ("SAMPLES_PER_FIFO", "FIFO_BLOCKS_PER_PACKET", "MAX_SAMPLES_PER_PACKET",
                 "MAX_TIMESYNC_LENGTH", "MAX_JSON_STRING_SIZE"):
        match = re.search(rf"^#define {name}\s+[^\n]+", source, re.MULTILINE)
        if match is None:
            raise RuntimeError(f"Production constant {name} not found")
        constants.append(match[0])
    version = (root / "neuralert/include/user_main/user_version.h").read_text()
    constants.extend(re.findall(r"^#define USER_(?:VERSION|SOFTWARE_PART_NUMBER)_STRING[^\n]+",
                                version, re.MULTILINE))
    with tempfile.TemporaryDirectory(prefix="packet-bounds-") as directory:
        build = Path(directory)
        (build / "packet_functions.inc").write_text("\n".join(functions))
        (build / "packet_constants.inc").write_text("\n".join(constants) + "\n")
        executable = build / "packet_bounds"
        for flags in ([], ["-DTEST_SMALL_PACKET"]):
            subprocess.run(
                [os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
                 "-g", "-fsanitize=address,undefined", *flags, "-I", str(build),
                 str(root / "tests/packet_bounds.c"), "-o", str(executable)], check=True
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
