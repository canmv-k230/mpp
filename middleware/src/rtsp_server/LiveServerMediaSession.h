#ifndef _LIVFRAMESERVERMEDIASUBSESSION_H
#define _LIVFRAMESERVERMEDIASUBSESSION_H

#include <functional>

#include "liveMedia.hh"
#include "OnDemandServerMediaSubsession.hh"
#include "LiveFrameSource.h"

class LiveServerMediaSession : public OnDemandServerMediaSubsession {
  public:
    using OnClientStreamFunc = std::function<void(unsigned clientSessionId, bool entering)>;

    static LiveServerMediaSession *createNew(UsageEnvironment &env, StreamReplicator *replicator,
                                              OnClientStreamFunc onClientStream = OnClientStreamFunc());

  protected:
    LiveServerMediaSession(UsageEnvironment &env, StreamReplicator *replicator, OnClientStreamFunc onClientStream);
    virtual ~LiveServerMediaSession();

  protected:
    virtual char const *getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource);
    virtual FramedSource *createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate);
    virtual RTPSink *createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource);
    virtual void startStream(unsigned clientSessionId, void* streamToken,
                              TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
                              unsigned short& rtpSeqNum, unsigned& rtpTimestamp,
                              ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                              void* serverRequestAlternativeByteHandlerClientData);
    virtual void deleteStream(unsigned clientSessionId, void*& streamToken);

  protected:
    StreamReplicator *fReplicator{nullptr};
    OnClientStreamFunc fOnClientStream;
};

#endif // _LIVFRAMESERVERMEDIASUBSESSION_H