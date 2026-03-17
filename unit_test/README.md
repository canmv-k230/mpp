# unit_test

This directory contains GoogleTest-based unit tests for sources under:

`mpp/mpp/middleware/src/`

Excluded 3rd party codes (per request): `ffmpeg`, `live555`, `mp4_player`, `mp4_format`, `x264`.

## Current coverage

Runnable source-level tests:
- `ogg/src/bitwise.c`
- `ogg/src/framing.c`
- `ogg/src/libogg.c`
- `rtsp_server/JpegFrameParser.cpp`
- `rtsp_server/LiveFrameSource.cpp`
- `rtsp_server/g711LiveFrameSource.cpp`
- `rtsp_server/h264LiveFrameSource.cpp`
- `rtsp_server/h265LiveFrameSource.cpp`
- `rtsp_pusher/rtsp_pusher.cpp` (with mocked `RTSPPusherImpl`)
- `rtsp_client/LiveFrameSource.cpp` (in separate binary)
- `rtsp_client/g711LiveFrameSource.cpp` (in separate binary)

API-contract tests:
- `kdmedia/include/media.h`
- `rtsp_client/include/rtsp_client.h`
- `rtsp_server/include/rtsp_server.h`

Remaining heavy integration files are tracked as TODO placeholders in `tests/test_integration_scaffold.cpp`.

## Run

```bash
cd mpp/middleware/src/unit_test
./compile_run.sh
```

`compile_run.sh` builds and runs:
- `all_tests` (core + kdmedia + rtsp_server + rtsp_pusher + API tests)
- `rtsp_client_tests` (separate binary to avoid symbol clashes with rtsp_server `LiveFrameSource`)
