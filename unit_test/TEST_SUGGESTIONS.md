# Unit Test Suggestions by Source File

Source root:
`/filesets/tatkin_tan/k230-src/relbuild/mpp/mpp/middleware/src`

## Runnable coverage now

| Source | Implemented tests | Notes |
|---|---|---|
| `ogg/src/bitwise.c` | bit packing/unpacking round-trip | host-runnable |
| `ogg/src/framing.c` | packet->page metadata checks | host-runnable |
| `ogg/src/libogg.c` | mux->demux callback round-trip + invalid params | host-runnable |
| `rtsp_server/JpegFrameParser.cpp` | valid JPEG parsing, invalid input, copy/assign semantics | host-runnable |
| `rtsp_server/LiveFrameSource.cpp` | exercised via h26x/g711 frame source tests with live555 mocks | mock-based |
| `rtsp_server/g711LiveFrameSource.cpp` | factory and encode type checks | mock-based |
| `rtsp_server/h264LiveFrameSource.cpp` | NAL parsing + SPS/PPS repeat behavior | mock-based |
| `rtsp_server/h265LiveFrameSource.cpp` | VPS/SPS/PPS parsing + repeat behavior | mock-based |
| `rtsp_pusher/rtsp_pusher.cpp` | queue/open/close/push behavior via mocked backend | mock-based |
| `rtsp_client/LiveFrameSource.cpp` | parse and queue-boundary behavior | mock-based, separate binary |
| `rtsp_client/g711LiveFrameSource.cpp` | factory and encode type checks | mock-based, separate binary |

## API-contract coverage now

| API Header | Implemented tests |
|---|---|
| `kdmedia/include/media.h` | default config values, non-copyability, callback interface contracts |
| `rtsp_client/include/rtsp_client.h` | init param defaults, callback wiring, non-copyability |
| `rtsp_server/include/rtsp_server.h` | session defaults, callback interface, non-copyability |

## Remaining TODO (integration-heavy)

| Source | Why still TODO | Suggested next tests |
|---|---|---|
| `kdmedia/media.cpp` | hardware SDK + device runtime | init/deinit error-path mapping for MPI calls, buffer lifecycle |
| `kdmedia/vo_cfg.cpp` | VO connector/device dependencies | connector selection and layer-attr composition tests with HAL mocks |
| `rtsp_pusher/RtspPusherImpl.cpp` | ffmpeg+network runtime | reconnect policy, timestamp continuity, SPS/PPS/keyframe handling |
| `rtsp_client/rtsp_client.cpp` | full live555 + network RTSP stack | DESCRIBE/SETUP/PLAY error branches and callback dispatch |
| `rtsp_server/mjpegStreamReplicator.cpp` | live555 runtime object model | replica fanout and deactivation flow |
| `rtsp_server/LiveServerMediaSession.cpp` | live555 session internals | aux SDP composition and sink/source creation |
| `rtsp_server/BackChannelServerMediaSubsession.cpp` | live555 RTP runtime | sink/source creation and packet callback flow |
| `rtsp_server/g711BackChannelServerMediaSubsession.cpp` | live555 RTP source runtime | PCMU RTP source behavior |
| `rtsp_server/mjpegMediaSubSession.cpp` | live555 RTP sink runtime | stream source creation and bitrate contract |
| `rtsp_server/rtsp_server.cpp` | full server loop and sockets | session create/destroy/url + data dispatch |
| `rtsp_server/mjpegLiveFrameSource.cpp` | JPEG source + live555 runtime | parser/scandata to packet mapping and truncation behavior |

## Inventory Summary

- Total target source files: `22`
- Runnable source-level coverage: `11`
- API-contract coverage (headers): `3`
- Remaining integration-heavy TODO sources: `11`
