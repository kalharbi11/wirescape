# SPDX-License-Identifier: GPL-3.0-only
"""Compare frozen Qt golden serialization with the current Qt-free renderer."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = "16e49e9302d3a3ceddaca1b52fe0fa963125ce4d"
SOURCE = "native/tests/golden_render.cpp"
BLOB = "8c136f28872a38024345427433afa905dbb1e318"


def run(*args, **kwargs):
    return subprocess.check_output(args, cwd=ROOT, **kwargs)


def hook(source):
    # project() also runs inside fetched dependencies. Inject only once at our root.
    native = (ROOT / "native").as_posix()
    return f'''if(NOT PROJECT_NAME STREQUAL "WirescapeNative" OR
   NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL "{native}")
  return()
endif()
if(TARGET golden_render_qt_frozen)
  return()
endif()
add_executable(golden_render_qt_frozen "{source.as_posix()}")
target_compile_features(golden_render_qt_frozen PRIVATE cxx_std_17)
target_compile_definitions(golden_render_qt_frozen PRIVATE QT_NO_KEYWORDS)
target_include_directories(golden_render_qt_frozen PRIVATE
  "{native}/document" "{native}/control")
target_link_libraries(golden_render_qt_frozen PRIVATE wirescape_document)
set_target_properties(golden_render_qt_frozen PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}")
function(require_frozen_qt_document)
  if(NOT TARGET wirescape_document)
    message(FATAL_ERROR "Qt Core document target unavailable for frozen comparison")
  endif()
endfunction()
cmake_language(DEFER CALL require_frozen_qt_document)
'''


def generator_options():
    cache = ROOT / "build/native/CMakeCache.txt"
    if not cache.is_file():
        return []
    values = dict(line.split("=", 1) for line in cache.read_text().splitlines()
                  if line.startswith(("CMAKE_GENERATOR:", "CMAKE_GENERATOR_PLATFORM:",
                                      "CMAKE_GENERATOR_TOOLSET:")))
    options = []
    for key, flag in (("CMAKE_GENERATOR", "-G"), ("CMAKE_GENERATOR_PLATFORM", "-A"),
                      ("CMAKE_GENERATOR_TOOLSET", "-T")):
        value = values.get(key + ":INTERNAL")
        if value:
            options.extend([flag, value])
    return options


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-prefix", type=Path, required=True)
    args = parser.parse_args()
    qt = args.qt_prefix.resolve()
    if not (qt / "lib/cmake/Qt6Core/Qt6CoreConfig.cmake").is_file():
        raise ValueError("Qt prefix must contain Qt6CoreConfig.cmake")
    frozen = run("git", "show", f"{BASE}:{SOURCE}")
    digest = hashlib.sha1(b"blob " + str(len(frozen)).encode() + b"\0" + frozen).hexdigest()
    if digest != BLOB:
        raise ValueError("frozen renderer blob differs from the reviewed immutable source")
    candidate = run("git", "rev-parse", "HEAD", text=True).strip()
    output = ROOT / "build_reports/goldens/qt-compare.json"
    build_root = ROOT / "build"
    build_root.mkdir(exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = str(qt / "bin") + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="qt-golden-", dir=build_root) as tmp:
        directory = Path(tmp)
        source = directory / "golden_render_qt_frozen.cpp"
        source.write_bytes(frozen)
        include = directory / "frozen.cmake"
        include.write_text(hook(source), encoding="utf-8")
        build = directory / "native"
        subprocess.check_call(["cmake", "-S", str(ROOT / "native"), "-B", str(build),
                               *generator_options(), "-DWIRESCAPE_BUILD_UI=OFF",
                               "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={qt}",
                               f"-DCMAKE_PROJECT_INCLUDE={include}"], cwd=ROOT, env=env)
        subprocess.check_call(["cmake", "--build", str(build), "--config", "Release",
                               "--target", "golden_render", "golden_render_qt_frozen",
                               "--parallel", "2"], cwd=ROOT, env=env)
        suffix = ".exe" if os.name == "nt" else ""
        current = next((p for p in (build / "Release", build)
                        if (p / ("golden_render" + suffix)).is_file()), None)
        if current is None:
            raise ValueError("Release golden renderers were not built")
        new, old = (current / (name + suffix)
                    for name in ("golden_render", "golden_render_qt_frozen"))
        for name, path in (("current", new), ("frozen", old)):
            print(f"{name} renderer sha256={hashlib.sha256(path.read_bytes()).hexdigest()}", flush=True)
        print(f"candidate={candidate} frozen-source={BASE}:{SOURCE} blob={digest}", flush=True)
        subprocess.check_call([sys.executable, str(ROOT / "tools/golden.py"), "check",
                               "--renderer", str(new), "--compare-renderer", str(old),
                               "--report", str(output)], cwd=ROOT, env=env)


if __name__ == "__main__":
    main()
