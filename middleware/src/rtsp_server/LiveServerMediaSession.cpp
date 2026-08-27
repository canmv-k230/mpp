#include <iostream>
#include <sstream>
#include "LiveServerMediaSession.h"
#include "LiveFrameSource.h"

LiveServerMediaSession *LiveServerMediaSession::createNew(UsageEnvironment &env, StreamReplicator *replicator,
                                                           OnClientStreamFunc onClientStream) {
    return new LiveServerMediaSession(env, replicator, std::move(onClientStream));
}

// For live streaming, we always set reuseFirstSource False, and use StreamReplicator,
//    so that createNewStreamSource() will be called when a new client comes.
//
LiveServerMediaSession::LiveServerMediaSession(UsageEnvironment &env, StreamReplicator *replicator, OnClientStreamFunc onClientStream)
    : OnDemandServerMediaSubsession(env, False/*reuseFirstSource*/), fReplicator(replicator), fOnClientStream(std::move(onClientStream))
    { }

LiveServerMediaSession::~LiveServerMediaSession() {
}

char const *LiveServerMediaSession::getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource) {
    const char *auxLine = NULL;
    std::string sdpLine =  ((LiveFrameSource *)(fReplicator->inputSource()))->getAuxLine();
    if (!sdpLine.empty()) {
        std::ostringstream os;
        os << "a=fmtp:" << (int)rtpSink->rtpPayloadType() << " ";
        os << sdpLine;
        os << "\r\n";
        auxLine = strdup(os.str().c_str());

        // std::cout << auxLine << std::endl;
    }
    return auxLine;
}

FramedSource *LiveServerMediaSession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) {
    EncodeType type = ((LiveFrameSource *)fReplicator->inputSource())->GetEncodeType();
    if (type == EncodeType::H264) {
        estBitrate = 20000;
        return H264VideoStreamDiscreteFramer::createNew(envir(), fReplicator->createStreamReplica());
    } else if (type == EncodeType::H265) {
        estBitrate = 20000;
        return H265VideoStreamDiscreteFramer::createNew(envir(), fReplicator->createStreamReplica());
    } else if (type == EncodeType::G711U) {
        estBitrate = 64;
        return fReplicator->createStreamReplica();
    } else {
        std::cout << "createNewStreamSource() -- type not supported yet" << (int) type << std::endl;
        return nullptr;
    }
}

RTPSink *LiveServerMediaSession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) {
    EncodeType type = ((LiveFrameSource *)fReplicator->inputSource())->GetEncodeType();
    if (type == EncodeType::H264) {
        OutPacketBuffer::increaseMaxSizeTo(2 * 1024 * 1024);
        return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (type == EncodeType::H265) {
        OutPacketBuffer::increaseMaxSizeTo(2 * 1024 * 1024);
        return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (type == EncodeType::G711U) {
        return SimpleRTPSink::createNew(envir(), rtpGroupsock, 0, 8000, "audio", "PCMU", 1, False /*allowMultipleFramesPerPacket*/);
    } else {
        std::cout << "createNewRTPSink() -- type not supported yet" << (int) type << std::endl;
        return nullptr;
    }
}

void LiveServerMediaSession::startStream(unsigned clientSessionId, void* streamToken,
                                          TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
                                          unsigned short& rtpSeqNum, unsigned& rtpTimestamp,
                                          ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                                          void* serverRequestAlternativeByteHandlerClientData) {
    OnDemandServerMediaSubsession::startStream(clientSessionId, streamToken, rtcpRRHandler, rtcpRRHandlerClientData,
                                                rtpSeqNum, rtpTimestamp, serverRequestAlternativeByteHandler,
                                                serverRequestAlternativeByteHandlerClientData);
    if (fOnClientStream) fOnClientStream(clientSessionId, true);
}

void LiveServerMediaSession::deleteStream(unsigned clientSessionId, void*& streamToken) {
    OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
    if (fOnClientStream) fOnClientStream(clientSessionId, false);
}
