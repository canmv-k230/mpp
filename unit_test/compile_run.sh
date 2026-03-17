#!/usr/bin/env bash
set -euo pipefail

UNIT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="${SRC_ROOT:-$(cd "$UNIT_ROOT/.." && pwd)}"
DEFAULT_WORK_ROOT="${WORK_ROOT:-${TMPDIR:-/tmp}/mpp_middleware_unit_test}"

BUILD_DIR="${BUILD_DIR:-$DEFAULT_WORK_ROOT/build}"
DEPS_DIR="${DEPS_DIR:-$DEFAULT_WORK_ROOT/.deps}"

CXX="${CXX:-g++}"
CC="${CC:-gcc}"
AR="${AR:-ar}"

if [[ ! -d "$SRC_ROOT" ]]; then
    echo "ERROR: SRC_ROOT not found: $SRC_ROOT" >&2
    exit 1
fi

search_gtest_under_root() {
    local root="$1"
    [[ -d "$root" ]] || return 1

    local hit
    hit="$(find "$root" -maxdepth 8 -type f \
        \( -path '*/gtest/src/gtest-all.cc' -o -path '*/googletest/src/gtest-all.cc' \) \
        2>/dev/null | head -n 1 || true)"

    if [[ -n "$hit" ]]; then
        echo "$(cd "$(dirname "$hit")/.." && pwd)"
        return 0
    fi

    return 1
}

find_gtest_root() {
    if [[ -n "${GTEST_ROOT:-}" && -f "${GTEST_ROOT}/src/gtest-all.cc" ]]; then
        echo "$GTEST_ROOT"
        return 0
    fi

    local candidates=(
        "$UNIT_ROOT/.deps/googletest/googletest"
        "$UNIT_ROOT/.deps/gtest"
        "/usr/src/googletest/googletest"
        "/usr/src/gtest"
    )
    local c
    for c in "${candidates[@]}"; do
        if [[ -f "$c/src/gtest-all.cc" ]]; then
            echo "$c"
            return 0
        fi
    done

    local src_tree_root
    src_tree_root="$(cd "$SRC_ROOT/../../../../.." 2>/dev/null && pwd || true)"

    local search_roots=(
        "$DEPS_DIR"
        "$UNIT_ROOT"
        "$SRC_ROOT"
    )
    if [[ -n "$src_tree_root" && -d "$src_tree_root" ]]; then
        search_roots+=("$src_tree_root")
    fi

    local root
    local found
    for root in "${search_roots[@]}"; do
        if found="$(search_gtest_under_root "$root")"; then
            echo "$found"
            return 0
        fi
    done

    return 1
}

install_gtest_local() {
    local install_root="$DEPS_DIR/googletest"
    mkdir -p "$DEPS_DIR"

    if ! command -v git >/dev/null 2>&1; then
        echo "ERROR: gtest not found and git is unavailable for auto-install." >&2
        return 1
    fi

    if [[ -d "$install_root/.git" ]]; then
        echo "Updating local googletest at: $install_root" >&2
        git -C "$install_root" pull --ff-only >/dev/null 2>&1 || true
    else
        echo "gtest not found. Installing googletest to: $install_root" >&2
        git clone --depth=1 https://github.com/google/googletest.git "$install_root"
    fi

    if [[ -f "$install_root/googletest/src/gtest-all.cc" ]]; then
        echo "$install_root/googletest"
        return 0
    fi

    if [[ -f "$install_root/src/gtest-all.cc" ]]; then
        echo "$install_root"
        return 0
    fi

    echo "ERROR: local googletest install is incomplete at: $install_root" >&2
    return 1
}

resolve_gtest_root() {
    local resolved_root
    if resolved_root="$(find_gtest_root)"; then
        echo "$resolved_root"
        return 0
    fi

    if resolved_root="$(install_gtest_local)"; then
        echo "$resolved_root"
        return 0
    fi

    echo "ERROR: failed to locate or install gtest. Set GTEST_ROOT manually." >&2
    return 1
}

GTEST_ROOT="$(resolve_gtest_root)"
export GTEST_ROOT

echo "Using SRC_ROOT: $SRC_ROOT"
echo "Using GTEST_ROOT: $GTEST_ROOT"
echo "Using BUILD_DIR: $BUILD_DIR"
echo "Using DEPS_DIR: $DEPS_DIR"

if [[ -z "${BUILD_DIR:-}" || "$BUILD_DIR" == "/" ]]; then
    echo "ERROR: invalid BUILD_DIR: $BUILD_DIR" >&2
    exit 1
fi

echo "[clean] Removing previous build artifacts from: $BUILD_DIR"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/obj" "$BUILD_DIR/lib" "$BUILD_DIR/bin"

COMMON_CXXFLAGS=(
    -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
    -include cstring
    -include unistd.h
)
COMMON_CFLAGS=(
    -O2 -Wall -Wextra -Wpedantic
)

GTEST_INCLUDES=(
    -I"$GTEST_ROOT"
    -I"$GTEST_ROOT/include"
)

MAIN_INCLUDES=(
    -I"$UNIT_ROOT/compat"
    -I"$UNIT_ROOT/compat/kdmedia"
    -I"$UNIT_ROOT/mocks/live555"
    -I"$SRC_ROOT/ogg/_include"
    -I"$SRC_ROOT/ogg/include"
    -I"$SRC_ROOT/kdmedia/include"
    -I"$SRC_ROOT/rtsp_server"
    -I"$SRC_ROOT/rtsp_server/include"
    -I"$SRC_ROOT/rtsp_client/include"
)

PUSHER_INCLUDES=(
    -I"$UNIT_ROOT/mocks/rtsp_pusher"
    -I"$SRC_ROOT/rtsp_pusher/include"
    -I"$SRC_ROOT/rtsp_pusher/_include"
)

CLIENT_INCLUDES=(
    -I"$UNIT_ROOT/compat"
    -I"$UNIT_ROOT/compat/kdmedia"
    -I"$UNIT_ROOT/mocks/live555"
    -I"$SRC_ROOT/rtsp_client"
    -I"$SRC_ROOT/rtsp_client/include"
)

echo "[1/8] Building gtest static libraries"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${GTEST_INCLUDES[@]}" \
    -c "$GTEST_ROOT/src/gtest-all.cc" \
    -o "$BUILD_DIR/obj/gtest-all.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${GTEST_INCLUDES[@]}" \
    -c "$GTEST_ROOT/src/gtest_main.cc" \
    -o "$BUILD_DIR/obj/gtest_main.o"
"$AR" rcs "$BUILD_DIR/lib/libgtest.a" "$BUILD_DIR/obj/gtest-all.o"
"$AR" rcs "$BUILD_DIR/lib/libgtest_main.a" "$BUILD_DIR/obj/gtest_main.o"

echo "[2/8] Building shared core and module sources"
"$CC" "${COMMON_CFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/ogg/src/bitwise.c" -o "$BUILD_DIR/obj/bitwise.o"
"$CC" "${COMMON_CFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/ogg/src/framing.c" -o "$BUILD_DIR/obj/framing.o"
"$CC" "${COMMON_CFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/ogg/src/libogg.c" -o "$BUILD_DIR/obj/libogg.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_server/JpegFrameParser.cpp" -o "$BUILD_DIR/obj/JpegFrameParser.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_server/LiveFrameSource.cpp" -o "$BUILD_DIR/obj/rtsp_server_LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_server/g711LiveFrameSource.cpp" -o "$BUILD_DIR/obj/rtsp_server_g711LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_server/h264LiveFrameSource.cpp" -o "$BUILD_DIR/obj/rtsp_server_h264LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${MAIN_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_server/h265LiveFrameSource.cpp" -o "$BUILD_DIR/obj/rtsp_server_h265LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${MAIN_INCLUDES[@]}" "${PUSHER_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_pusher/rtsp_pusher.cpp" -o "$BUILD_DIR/obj/rtsp_pusher_rtsp_pusher.o"

echo "[3/8] Building main test files"
for test_src in \
    test_integration_scaffold.cpp \
    test_jpeg_frame_parser.cpp \
    test_kdmedia_api.cpp \
    test_ogg.cpp \
    test_rtsp_client_api.cpp \
    test_rtsp_server_api.cpp \
    test_rtsp_server_h26x.cpp; do
    test_name="${test_src%.cpp}"
    "$CXX" "${COMMON_CXXFLAGS[@]}" "${GTEST_INCLUDES[@]}" "${MAIN_INCLUDES[@]}" \
        -c "$UNIT_ROOT/tests/$test_src" \
        -o "$BUILD_DIR/obj/${test_name}.o"
done

"$CXX" "${COMMON_CXXFLAGS[@]}" "${GTEST_INCLUDES[@]}" "${MAIN_INCLUDES[@]}" "${PUSHER_INCLUDES[@]}" \
    -c "$UNIT_ROOT/tests/test_rtsp_pusher_runtime.cpp" \
    -o "$BUILD_DIR/obj/test_rtsp_pusher_runtime.o"

echo "[4/8] Linking main test binary"
"$CXX" -pthread \
    "$BUILD_DIR/obj/test_integration_scaffold.o" \
    "$BUILD_DIR/obj/test_jpeg_frame_parser.o" \
    "$BUILD_DIR/obj/test_kdmedia_api.o" \
    "$BUILD_DIR/obj/test_ogg.o" \
    "$BUILD_DIR/obj/test_rtsp_client_api.o" \
    "$BUILD_DIR/obj/test_rtsp_pusher_runtime.o" \
    "$BUILD_DIR/obj/test_rtsp_server_api.o" \
    "$BUILD_DIR/obj/test_rtsp_server_h26x.o" \
    "$BUILD_DIR/obj/bitwise.o" \
    "$BUILD_DIR/obj/framing.o" \
    "$BUILD_DIR/obj/libogg.o" \
    "$BUILD_DIR/obj/JpegFrameParser.o" \
    "$BUILD_DIR/obj/rtsp_server_LiveFrameSource.o" \
    "$BUILD_DIR/obj/rtsp_server_g711LiveFrameSource.o" \
    "$BUILD_DIR/obj/rtsp_server_h264LiveFrameSource.o" \
    "$BUILD_DIR/obj/rtsp_server_h265LiveFrameSource.o" \
    "$BUILD_DIR/obj/rtsp_pusher_rtsp_pusher.o" \
    "$BUILD_DIR/lib/libgtest_main.a" \
    "$BUILD_DIR/lib/libgtest.a" \
    -o "$BUILD_DIR/bin/all_tests"

echo "[5/8] Building rtsp_client sources"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${CLIENT_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_client/LiveFrameSource.cpp" -o "$BUILD_DIR/obj/rtsp_client_LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${CLIENT_INCLUDES[@]}" -c "$SRC_ROOT/rtsp_client/g711LiveFrameSource.cpp" -o "$BUILD_DIR/obj/rtsp_client_g711LiveFrameSource.o"

echo "[6/8] Building rtsp_client test files"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${GTEST_INCLUDES[@]}" "${CLIENT_INCLUDES[@]}" \
    -c "$UNIT_ROOT/tests/test_rtsp_client_liveframesource.cpp" \
    -o "$BUILD_DIR/obj/test_rtsp_client_liveframesource.o"

echo "[7/8] Linking rtsp_client test binary"
"$CXX" -pthread \
    "$BUILD_DIR/obj/test_rtsp_client_liveframesource.o" \
    "$BUILD_DIR/obj/rtsp_client_LiveFrameSource.o" \
    "$BUILD_DIR/obj/rtsp_client_g711LiveFrameSource.o" \
    "$BUILD_DIR/lib/libgtest_main.a" \
    "$BUILD_DIR/lib/libgtest.a" \
    -o "$BUILD_DIR/bin/rtsp_client_tests"

echo "[8/8] Running unit tests"
"$BUILD_DIR/bin/all_tests" --gtest_color=yes "$@"
"$BUILD_DIR/bin/rtsp_client_tests" --gtest_color=yes "$@"
