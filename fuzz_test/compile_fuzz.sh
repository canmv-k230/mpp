#!/usr/bin/env bash
set -euo pipefail

FUZZ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="${SRC_ROOT:-$(cd "$FUZZ_ROOT/.." && pwd)}"
BUILD_DIR="${BUILD_DIR:-$FUZZ_ROOT/build}"
OBJ_DIR="$BUILD_DIR/obj"
BIN_DIR="$BUILD_DIR/bin"
CORPUS_DIR="${CORPUS_DIR:-$FUZZ_ROOT/corpus}"

CXX="${CXX:-clang++}"
CC="${CC:-clang}"

if [[ ! -d "$SRC_ROOT" ]]; then
    echo "ERROR: SRC_ROOT not found: $SRC_ROOT" >&2
    exit 1
fi

if [[ -z "$BUILD_DIR" || "$BUILD_DIR" == "/" ]]; then
    echo "ERROR: invalid BUILD_DIR: $BUILD_DIR" >&2
    exit 1
fi

resolve_sudo() {
    if [[ "$(id -u)" -eq 0 ]]; then
        echo ""
        return 0
    fi
    if command -v sudo >/dev/null 2>&1; then
        echo "sudo"
        return 0
    fi
    echo "ERROR: need root privileges for package install but sudo is unavailable." >&2
    return 1
}

install_fuzzer_env() {
    local runner
    runner="$(resolve_sudo)"

    if command -v apt-get >/dev/null 2>&1; then
        echo "Installing fuzzing toolchain via apt-get..."
        ${runner:+$runner }apt-get update

        local apt_pkgs=(clang llvm lld build-essential)
        local optional_pkgs=(
            libfuzzer-dev
            libfuzzer-14-dev
            compiler-rt
            libclang-rt-dev
            libclang-rt-14-dev
        )
        local pkg
        for pkg in "${optional_pkgs[@]}"; do
            if apt-cache show "$pkg" >/dev/null 2>&1; then
                apt_pkgs+=("$pkg")
            fi
        done

        ${runner:+$runner }env DEBIAN_FRONTEND=noninteractive apt-get install -y "${apt_pkgs[@]}"
        return 0
    fi

    if command -v dnf >/dev/null 2>&1; then
        echo "Installing fuzzing toolchain via dnf..."
        ${runner:+$runner }dnf install -y clang llvm lld compiler-rt
        return 0
    fi

    if command -v yum >/dev/null 2>&1; then
        echo "Installing fuzzing toolchain via yum..."
        ${runner:+$runner }yum install -y clang llvm lld compiler-rt
        return 0
    fi

    if command -v pacman >/dev/null 2>&1; then
        echo "Installing fuzzing toolchain via pacman..."
        ${runner:+$runner }pacman -Sy --noconfirm clang llvm lld compiler-rt
        return 0
    fi

    echo "ERROR: no supported package manager found for automatic fuzz environment install." >&2
    return 1
}

FUZZ_TOOLCHAIN_EXTRA_FLAGS=()
FUZZ_STDLIB_INCLUDE_FLAGS=()

discover_gxx_compat_flags() {
    FUZZ_TOOLCHAIN_EXTRA_FLAGS=()
    FUZZ_STDLIB_INCLUDE_FLAGS=()

    if ! command -v g++ >/dev/null 2>&1; then
        return 1
    fi

    local libstdcpp_path
    libstdcpp_path="$(g++ -print-file-name=libstdc++.so 2>/dev/null || true)"
    if [[ -z "$libstdcpp_path" || "$libstdcpp_path" == "libstdc++.so" || ! -f "$libstdcpp_path" ]]; then
        return 1
    fi

    local gcc_libdir
    gcc_libdir="$(dirname "$libstdcpp_path")"
    FUZZ_TOOLCHAIN_EXTRA_FLAGS=("-B$gcc_libdir" "-L$gcc_libdir")

    local gcc_version triplet
    gcc_version="$(basename "$gcc_libdir")"
    triplet="$(g++ -dumpmachine 2>/dev/null || echo x86_64-linux-gnu)"

    local include_candidates=(
        "/usr/include/c++/$gcc_version"
        "/usr/include/$triplet/c++/$gcc_version"
        "/usr/include/c++/$gcc_version/backward"
    )
    local d
    for d in "${include_candidates[@]}"; do
        if [[ -d "$d" ]]; then
            FUZZ_STDLIB_INCLUDE_FLAGS+=("-isystem" "$d")
        fi
    done

    return 0
}

probe_libfuzzer() {
    local probe_dir probe_src probe_bin
    probe_dir="$(mktemp -d)"
    probe_src="$probe_dir/probe.cpp"
    probe_bin="$probe_dir/probe"

    cat > "$probe_src" <<'EOS'
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t*, size_t) { return 0; }
EOS

    if "$CXX" -std=c++17 -O1 -g -fno-omit-frame-pointer \
        -fsanitize=fuzzer,address "$probe_src" -o "$probe_bin" >/dev/null 2>&1; then
        rm -rf "$probe_dir"
        return 0
    fi

    if discover_gxx_compat_flags; then
        if "$CXX" -std=c++17 -O1 -g -fno-omit-frame-pointer \
            "${FUZZ_STDLIB_INCLUDE_FLAGS[@]}" "${FUZZ_TOOLCHAIN_EXTRA_FLAGS[@]}" \
            -fsanitize=fuzzer,address "$probe_src" -o "$probe_bin" >/dev/null 2>&1; then
            rm -rf "$probe_dir"
            return 0
        fi
    fi

    rm -rf "$probe_dir"
    return 1
}

ensure_fuzzer_environment() {
    FUZZ_TOOLCHAIN_EXTRA_FLAGS=()
    FUZZ_STDLIB_INCLUDE_FLAGS=()

    if command -v "$CXX" >/dev/null 2>&1 && command -v "$CC" >/dev/null 2>&1; then
        if probe_libfuzzer; then
            return 0
        fi
    fi

    echo "libFuzzer toolchain is missing or incomplete. Attempting auto-install..."
    install_fuzzer_env

    CXX="${CXX:-clang++}"
    CC="${CC:-clang}"

    FUZZ_TOOLCHAIN_EXTRA_FLAGS=()
    FUZZ_STDLIB_INCLUDE_FLAGS=()

    if ! command -v "$CXX" >/dev/null 2>&1 || ! command -v "$CC" >/dev/null 2>&1; then
        echo "ERROR: clang/clang++ not available after installation." >&2
        return 1
    fi

    if ! probe_libfuzzer; then
        echo "ERROR: libFuzzer still not available after installation." >&2
        return 1
    fi

    return 0
}

echo "Using FUZZ_ROOT: $FUZZ_ROOT"
echo "Using SRC_ROOT: $SRC_ROOT"
echo "Using BUILD_DIR: $BUILD_DIR"

echo "[clean] Removing previous build artifacts from: $BUILD_DIR"
rm -rf "$BUILD_DIR"
mkdir -p "$OBJ_DIR" "$BIN_DIR"

ensure_fuzzer_environment

echo "Using CXX: $CXX"
echo "Using CC: $CC"
if [[ ${#FUZZ_STDLIB_INCLUDE_FLAGS[@]} -gt 0 ]]; then
    echo "Using stdlib include flags: ${FUZZ_STDLIB_INCLUDE_FLAGS[*]}"
fi
if [[ ${#FUZZ_TOOLCHAIN_EXTRA_FLAGS[@]} -gt 0 ]]; then
    echo "Using toolchain extra flags: ${FUZZ_TOOLCHAIN_EXTRA_FLAGS[*]}"
fi

COMMON_CXXFLAGS=(
    -std=c++17
    -O1
    -g
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
    -Wall
    -Wextra
    -Wpedantic
    -pthread
    -include cstring
    -include unistd.h
    "${FUZZ_STDLIB_INCLUDE_FLAGS[@]}"
    "${FUZZ_TOOLCHAIN_EXTRA_FLAGS[@]}"
)

COMMON_CFLAGS=(
    -O1
    -g
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
    -Wall
    -Wextra
    -Wpedantic
    "${FUZZ_TOOLCHAIN_EXTRA_FLAGS[@]}"
)

SAN_COMPILE_FLAGS=(
    -fsanitize=fuzzer-no-link,address,undefined
)

SAN_LINK_FLAGS=(
    -fsanitize=fuzzer,address,undefined
)

INCLUDE_COMPAT_OGG=(
    -I"$FUZZ_ROOT/compat"
    -I"$SRC_ROOT/ogg/_include"
    -I"$SRC_ROOT/ogg/include"
)

INCLUDE_COMPAT_KDMEDIA=(
    -I"$FUZZ_ROOT/compat/kdmedia"
    -I"$SRC_ROOT/kdmedia/include"
)

INCLUDE_LIVE555=(
    -I"$FUZZ_ROOT/mocks/live555"
)

INCLUDE_RTSP_SERVER=(
    -I"$SRC_ROOT/rtsp_server"
    -I"$SRC_ROOT/rtsp_server/include"
)

INCLUDE_RTSP_CLIENT=(
    -I"$SRC_ROOT/rtsp_client"
    -I"$SRC_ROOT/rtsp_client/include"
)

INCLUDE_RTSP_PUSHER=(
    -I"$FUZZ_ROOT/mocks/rtsp_pusher"
    -I"$SRC_ROOT/rtsp_pusher/include"
    -I"$SRC_ROOT/rtsp_pusher/_include"
)

echo "[1/8] Building common module objects"
"$CC" "${COMMON_CFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_OGG[@]}" -c "$SRC_ROOT/ogg/src/bitwise.c" -o "$OBJ_DIR/ogg_bitwise.o"
"$CC" "${COMMON_CFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_OGG[@]}" -c "$SRC_ROOT/ogg/src/framing.c" -o "$OBJ_DIR/ogg_framing.o"
"$CC" "${COMMON_CFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_OGG[@]}" -c "$SRC_ROOT/ogg/src/libogg.c" -o "$OBJ_DIR/ogg_libogg.o"

"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_RTSP_SERVER[@]}" -c "$SRC_ROOT/rtsp_server/JpegFrameParser.cpp" -o "$OBJ_DIR/JpegFrameParser.o"

"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_SERVER[@]}" -c "$SRC_ROOT/rtsp_server/LiveFrameSource.cpp" -o "$OBJ_DIR/rtsp_server_LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_SERVER[@]}" -c "$SRC_ROOT/rtsp_server/g711LiveFrameSource.cpp" -o "$OBJ_DIR/rtsp_server_g711LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_SERVER[@]}" -c "$SRC_ROOT/rtsp_server/h264LiveFrameSource.cpp" -o "$OBJ_DIR/rtsp_server_h264LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_SERVER[@]}" -c "$SRC_ROOT/rtsp_server/h265LiveFrameSource.cpp" -o "$OBJ_DIR/rtsp_server_h265LiveFrameSource.o"

"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_CLIENT[@]}" -c "$SRC_ROOT/rtsp_client/LiveFrameSource.cpp" -o "$OBJ_DIR/rtsp_client_LiveFrameSource.o"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_CLIENT[@]}" -c "$SRC_ROOT/rtsp_client/g711LiveFrameSource.cpp" -o "$OBJ_DIR/rtsp_client_g711LiveFrameSource.o"

"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_COMPILE_FLAGS[@]}" "${INCLUDE_RTSP_PUSHER[@]}" -c "$SRC_ROOT/rtsp_pusher/rtsp_pusher.cpp" -o "$OBJ_DIR/rtsp_pusher.o"

echo "[2/8] Linking fuzz_jpeg_parser"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_LINK_FLAGS[@]}" "${INCLUDE_RTSP_SERVER[@]}" \
    "$FUZZ_ROOT/harnesses/fuzz_jpeg_parser.cpp" "$OBJ_DIR/JpegFrameParser.o" -o "$BIN_DIR/fuzz_jpeg_parser"

echo "[3/8] Linking fuzz_ogg_mux_demux"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_LINK_FLAGS[@]}" "${INCLUDE_COMPAT_OGG[@]}" \
    "$FUZZ_ROOT/harnesses/fuzz_ogg_mux_demux.cpp" \
    "$OBJ_DIR/ogg_bitwise.o" "$OBJ_DIR/ogg_framing.o" "$OBJ_DIR/ogg_libogg.o" \
    -o "$BIN_DIR/fuzz_ogg_mux_demux"

echo "[4/8] Linking fuzz_rtsp_server_h26x"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_LINK_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_SERVER[@]}" \
    "$FUZZ_ROOT/harnesses/fuzz_rtsp_server_h26x.cpp" \
    "$OBJ_DIR/rtsp_server_LiveFrameSource.o" "$OBJ_DIR/rtsp_server_g711LiveFrameSource.o" \
    "$OBJ_DIR/rtsp_server_h264LiveFrameSource.o" "$OBJ_DIR/rtsp_server_h265LiveFrameSource.o" \
    -o "$BIN_DIR/fuzz_rtsp_server_h26x"

echo "[5/8] Linking fuzz_rtsp_client_liveframe"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_LINK_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" "${INCLUDE_LIVE555[@]}" "${INCLUDE_RTSP_CLIENT[@]}" \
    "$FUZZ_ROOT/harnesses/fuzz_rtsp_client_liveframe.cpp" \
    "$OBJ_DIR/rtsp_client_LiveFrameSource.o" "$OBJ_DIR/rtsp_client_g711LiveFrameSource.o" \
    -o "$BIN_DIR/fuzz_rtsp_client_liveframe"

echo "[6/8] Linking fuzz_rtsp_pusher"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_LINK_FLAGS[@]}" "${INCLUDE_RTSP_PUSHER[@]}" \
    "$FUZZ_ROOT/harnesses/fuzz_rtsp_pusher.cpp" "$OBJ_DIR/rtsp_pusher.o" -o "$BIN_DIR/fuzz_rtsp_pusher"

echo "[7/8] Linking fuzz_kdmedia_config"
"$CXX" "${COMMON_CXXFLAGS[@]}" "${SAN_LINK_FLAGS[@]}" "${INCLUDE_COMPAT_KDMEDIA[@]}" \
    "$FUZZ_ROOT/harnesses/fuzz_kdmedia_config.cpp" -o "$BIN_DIR/fuzz_kdmedia_config"

echo "[8/8] Build complete"
echo "Fuzz binaries generated under: $BIN_DIR"
echo "Examples:"
echo "  $BIN_DIR/fuzz_jpeg_parser $CORPUS_DIR/jpeg -max_total_time=30"
echo "  $BIN_DIR/fuzz_ogg_mux_demux $CORPUS_DIR/ogg -max_total_time=30"
echo "  $BIN_DIR/fuzz_rtsp_server_h26x $CORPUS_DIR/rtsp_server_h26x -max_total_time=30"
echo "  $BIN_DIR/fuzz_rtsp_client_liveframe $CORPUS_DIR/rtsp_client -max_total_time=30"
echo "  $BIN_DIR/fuzz_rtsp_pusher $CORPUS_DIR/rtsp_pusher -max_total_time=30"
echo "  $BIN_DIR/fuzz_kdmedia_config $CORPUS_DIR/kdmedia -max_total_time=30"
