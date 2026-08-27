#ifndef _MJPEG_MEDIASUBSESSION_H
#define _MJPEG_MEDIASUBSESSION_H

#include <functional>

#include "liveMedia.hh"
#include "OnDemandServerMediaSubsession.hh"
#include "FramedSource.hh"
#include "mjpegStreamReplicator.h"

class MjpegMediaSubsession: public OnDemandServerMediaSubsession {
  public:
    using OnClientStreamFunc = std::function<void(unsigned clientSessionId, bool entering)>;

    static MjpegMediaSubsession *createNew(UsageEnvironment &env, JpegStreamReplicator *replicator,
                                            OnClientStreamFunc onClientStream = OnClientStreamFunc());

  protected:
    MjpegMediaSubsession(UsageEnvironment& env, JpegStreamReplicator *replicator, OnClientStreamFunc onClientStream);
    ~MjpegMediaSubsession();

  protected:
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource);
    virtual void startStream(unsigned clientSessionId, void* streamToken,
                              TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
                              unsigned short& rtpSeqNum, unsigned& rtpTimestamp,
                              ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                              void* serverRequestAlternativeByteHandlerClientData);
    virtual void deleteStream(unsigned clientSessionId, void*& streamToken);

  protected:
    JpegStreamReplicator *fReplicator{nullptr};
    OnClientStreamFunc fOnClientStream;
};

#endif // _MJPEG_MEDIASUBSESSION_H