#include "mjpegMediaSubSession.h"
#include "FramedSource.hh"
#include "mjpegLiveFrameSource.h"
#include "JPEGVideoRTPSink.hh"

MjpegMediaSubsession *MjpegMediaSubsession::createNew(UsageEnvironment &env, JpegStreamReplicator *replicator,
                                                       OnClientStreamFunc onClientStream) {
    return new MjpegMediaSubsession(env, replicator, std::move(onClientStream));
}

MjpegMediaSubsession::MjpegMediaSubsession(UsageEnvironment& env, JpegStreamReplicator *replicator, OnClientStreamFunc onClientStream)
    : OnDemandServerMediaSubsession(env, False/*reuseFirstSource*/), fReplicator(replicator), fOnClientStream(std::move(onClientStream))
    { }

MjpegMediaSubsession::~MjpegMediaSubsession() {
}

FramedSource* MjpegMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
    if (fReplicator) {
        // std::cout << "MjpegMediaSubsession::createNewStreamSource replicator called" << std::endl;
        estBitrate = 30000;
        MjpegLiveVideoSource *frameSource = fReplicator->createStreamReplica();
        return frameSource;
    }
    return nullptr;
}

RTPSink* MjpegMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource){
    // std::cout << "MjpegMediaSubsession::createNewRTPSink called"<< std::endl;
    OutPacketBuffer::increaseMaxSizeTo(2 * 1024 * 1024);
    return JPEGVideoRTPSink::createNew(envir(), rtpGroupsock);
}

void MjpegMediaSubsession::startStream(unsigned clientSessionId, void* streamToken,
                                        TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
                                        unsigned short& rtpSeqNum, unsigned& rtpTimestamp,
                                        ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                                        void* serverRequestAlternativeByteHandlerClientData) {
    OnDemandServerMediaSubsession::startStream(clientSessionId, streamToken, rtcpRRHandler, rtcpRRHandlerClientData,
                                                rtpSeqNum, rtpTimestamp, serverRequestAlternativeByteHandler,
                                                serverRequestAlternativeByteHandlerClientData);
    if (fOnClientStream) fOnClientStream(clientSessionId, true);
}

void MjpegMediaSubsession::deleteStream(unsigned clientSessionId, void*& streamToken) {
    OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
    if (fOnClientStream) fOnClientStream(clientSessionId, false);
}
