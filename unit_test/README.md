# Host unit tests

Run these commands from the MPP repository root (the directory containing
`kernel/`, `middleware/` and `unit_test/`). No board, firmware image or private
RTL source is required.

## Middleware tests

Requirements: Bash, a C compiler, a C++17 compiler, `ar`, pthreads and GoogleTest.

```sh
./unit_test/compile_run.sh
```

The script locates GoogleTest or downloads it from its public upstream repository
if unavailable. To use an existing installation without downloading, set
`GTEST_ROOT` to the directory containing `src/gtest-all.cc` and `include/gtest/`.
`CC`, `CXX`, `AR`, `BUILD_DIR`, `DEPS_DIR` and `SRC_ROOT` can also be overridden.
The default source directory is `middleware/src`; build/dependency files go under
`${TMPDIR:-/tmp}/mpp_middleware_unit_test`. The build directory is recreated on
runs, so use a dedicated directory when overriding `BUILD_DIR`.

The script builds and runs:

- `all_tests`: core media, RTSP server/pusher and API contract tests.
- `rtsp_client_tests`: a separate binary to avoid symbol clashes between client
  and server `LiveFrameSource` implementations.

Runnable source coverage includes:

- Ogg `bitwise.c`, `framing.c` and `libogg.c`.
- RTSP server `JpegFrameParser.cpp`, `LiveFrameSource.cpp`,
  `g711LiveFrameSource.cpp`, `h264LiveFrameSource.cpp` and
  `h265LiveFrameSource.cpp`.
- RTSP pusher `rtsp_pusher.cpp`, using a mocked backend.
- RTSP client `LiveFrameSource.cpp` and `g711LiveFrameSource.cpp`.

API contract checks cover `kdmedia/include/media.h`,
`rtsp_client/include/rtsp_client.h` and `rtsp_server/include/rtsp_server.h`.
The runtime tests use mocks where device or network services are required;
passing them does not validate a real camera, encoder or RTSP connection.
Integration placeholders in `tests/test_integration_scaffold.cpp` are disabled
TODOs, not completed coverage. See [TEST_SUGGESTIONS.md](TEST_SUGGESTIONS.md).
