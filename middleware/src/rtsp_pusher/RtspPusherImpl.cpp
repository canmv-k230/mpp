#include "assert.h"
#include "RtspPusherImpl.h"
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
}

bool  RTSPPusherImpl::m_bFfmpegInit = false;
static void SPSPPSPrint3(const char* strInfo, char* pBuf, int nLen)
{
	char strSPSPPS[1024] = { 0 };
	if (nLen <= 1024)
	{
		for (int i = 0; i < nLen; i++)
		{
			sprintf(strSPSPPS + 3 * i, "%02x ", (uint8_t)pBuf[i]);
		}
		printf("-----%s: %s\n", strInfo, strSPSPPS);
	}
}

RTSPPusherImpl::RTSPPusherImpl()
{
	if (!m_bFfmpegInit)
	{
		av_register_all();
		avformat_network_init();
		m_bFfmpegInit = true;
	}

	m_nSPSPPSLen = 0;
	m_bInited = false;
	m_bNeedIframe = true;
	m_lVTimeStamp = 0;
	m_lATimeStamp = 0;
	m_nPushFrameFailCnt = 0;

	m_sTransport[0] = '\0';
	m_nVideoWidth = 0;
	m_nVideoHeight = 0;
		m_nfps = 25;
		m_start_reconnect = false;
		m_hConnectThread = 0;
		outputContext = NULL;
		videoCodecContext = NULL;
		m_semInited = false;
		m_mutexInited = false;
	}
int RTSPPusherImpl::init(const char* rtspurl, int video_width, int video_height, int fps, const char* transport)
{
		if (rtspurl == NULL || strlen(rtspurl) >= sizeof(m_sUrl))
		{
			printf("rtsp url invalid or too long\n");
			return -1;
		}
	strncpy(m_sUrl, rtspurl, sizeof(m_sUrl) - 1);
	m_sUrl[sizeof(m_sUrl) - 1] = '\0';
	m_nfps = (fps > 0 && fps <= 120) ? fps : 25;
	if (transport != NULL && strcmp(transport, "udp") == 0)
	{
		strncpy(m_sTransport, "udp", sizeof(m_sTransport) - 1);
	}
	else
	{
		strncpy(m_sTransport, "tcp", sizeof(m_sTransport) - 1);
	}
	m_sTransport[sizeof(m_sTransport) - 1] = '\0';
		m_nVideoWidth = video_width;
		m_nVideoHeight = video_height;

		printf("zlmedia url: %s, w: %d, h: %d, fps: %d, transport: %s\n", m_sUrl, m_nVideoWidth, m_nVideoHeight, m_nfps, m_sTransport);

		if (!m_semInited && sem_init(&m_sConnect, 0, 0) != 0)
		{
			printf("sem_init failed\n");
			return -1;
		}
		m_semInited = true;
		if (!m_mutexInited && pthread_mutex_init(&m_Lock, NULL) != 0)
		{
			printf("pthread_mutex_init failed\n");
			return -1;
		}
		m_mutexInited = true;
	m_start_reconnect = true;
	pthread_create(&m_hConnectThread, NULL, ReconnectThread, this);

	return 0;
}

int RTSPPusherImpl::open()
{
	m_bInited = false;
	m_bNeedIframe = true;
	m_lVTimeStamp = 0;
	m_lATimeStamp = 0;
	int  nRet = _openVideoOutput();
	if (0 != nRet)
	{
		//assert(false);
		printf("open video output failed\n");
		close();
		return -1;
	}
		nRet = _OpenRtspStreams();
		if (0 != nRet)
		{
			printf("open rtsp streams failed\n");
			//assert(false);
			close();
			return -1;
		}
	printf("rtsp pusher init success, url: %s\n", m_sUrl);
	m_bInited = true;
	return 0;
}

int RTSPPusherImpl::_openVideoOutput()
{
	avformat_alloc_output_context2(&outputContext, 0, "rtsp", m_sUrl);
	if (!outputContext)
		return -1;

	av_dict_set(&outputContext->metadata, "rtsp_transport", m_sTransport, 0);
	av_dict_set(&outputContext->metadata, "buffer_size", "4096000", 0);
	av_dict_set(&outputContext->metadata, "stimeout", "5000000", 0);

	av_opt_set(outputContext->priv_data, "rtsp_transport", m_sTransport, 0);

	// Video encoder setup
	AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!videoCodec)
	{
		printf("=========avcodec_find_encoder failed\n");
		return -1;
	}


	videoCodecContext = avcodec_alloc_context3(videoCodec);
	if (!videoCodecContext)
	{
		printf("=========avcodec_alloc_context3 failed\n");
		return -1;
	}

	// Set video codec parameters
	videoCodecContext->width = m_nVideoWidth;
	videoCodecContext->height = m_nVideoHeight;
	videoCodecContext->framerate = { m_nfps, 1 };
	videoCodecContext->gop_size = m_nfps;
	videoCodecContext->time_base = { 1, m_nfps };
	videoCodecContext->max_b_frames = 0;
	videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
	videoCodecContext->bit_rate = 20000000;
	videoCodecContext->rc_min_rate = 20000000;
	videoCodecContext->rc_max_rate = 20000000;
	videoCodecContext->rc_buffer_size = 20000000;
	videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

	AVDictionary* param = nullptr;
	if (AV_CODEC_ID_H264 == videoCodecContext->codec_id)
	{
		av_dict_set(&param, "preset", "medium", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);
		//av_dict_set(&param, "profile", "high", 0);
	}
	else if (AV_CODEC_ID_H265 == videoCodecContext->codec_id)
	{
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zero-latency", 0);
	}

	int nRet = avcodec_open2(videoCodecContext, videoCodec, &param);
	if (nRet < 0)
	{
		// Handle error - could not open video codec
		char log[256];
		av_strerror(nRet, log, 256);
		printf("=========avcodec_open2 failed, %s\n", log);
		return -1;
	}

	// {
	// 	//memcpy(videoCodecContext->extradata, m_pSPSPPS, m_nSPSPPSLen);
	// 	videoCodecContext->extradata = (uint8_t*)m_pSPSPPS;
	// 	videoCodecContext->extradata_size = m_nSPSPPSLen;
	// }

	if (videoCodecContext->extradata)
		printf("url: %s pusher extradata: %p, size: %d\n", m_sUrl, videoCodecContext->extradata, videoCodecContext->extradata_size);
	return 0;
}

int RTSPPusherImpl::_OpenRtspStreams()
{
	int nRet = -1;
	do
	{
		if (!outputContext)
			break;

		videoStream = avformat_new_stream(outputContext, 0);
		if (!videoStream)
			break;
		videoStream->id = outputContext->nb_streams - 1;
		{
			if (avcodec_copy_context(videoStream->codec, videoCodecContext) < 0) {
				printf("Fail: avcodec_copy_context\n");
				return -1;
			}
			videoStream->codec->codec_tag = 0;
			videoStream->time_base = videoCodecContext->time_base;
			videoStream->avg_frame_rate = videoCodecContext->framerate;

			if (outputContext->oformat->flags & AVFMT_GLOBALHEADER)
				videoStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			avcodec_parameters_from_context(videoStream->codecpar, videoCodecContext);
		}

		printf("extradata: %p, size: %d\n", videoStream->codecpar->extradata, videoStream->codecpar->extradata_size);
		SPSPPSPrint3(m_sUrl, (char*)videoStream->codecpar->extradata, videoStream->codecpar->extradata_size);
		nRet = _ConnectRtspServer();

	} while (0);
	if (nRet != 0)
	{
		// assert(false);
		//_CloseRtspPusher();
		sem_post(&m_sConnect);
	}
	return nRet;
}

int RTSPPusherImpl::_ConnectRtspServer()
{
	assert(NULL != outputContext);

	// while (m_start_reconnect)
	// {
	// 	int nRet = avformat_write_header(outputContext, nullptr);
	// 	if (nRet < 0)
	// 	{
	// 		printf("avformat_write_header failed, Ret: %d, url: %s\n", nRet, m_sUrl);
	// 		sleep(1);
	// 		continue;
	// 	}
	// 	break;
	// }


	//return 0;

	AVDictionary* opts = nullptr;
	av_dict_set(&opts, "rtsp_transport", m_sTransport, 0);
	av_dict_set(&opts, "buffer_size", "4194304", 0);
	av_dict_set(&opts, "stimeout", "5000000", 0);
	av_dict_set(&opts, "muxdelay", "0", 0);
	int ret = avformat_write_header(outputContext, &opts);
	av_dict_free(&opts);
	return ret;
}

void RTSPPusherImpl::_CloseRtspStreams()
{
	if (outputContext)
	{
		av_write_trailer(outputContext);
		avio_closep(&outputContext->pb);
	}
}

	void RTSPPusherImpl::setSPSPPS(char* pSPSBuf, int nSPSLen, char* pPPSBuf, int nPPSLen)
	{
		if (nSPSLen < 0 || nPPSLen < 0 || nSPSLen + nPPSLen > (int)sizeof(m_pSPSPPS))
		{
			printf("setSPSPPS invalid size, sps:%d pps:%d max:%d\n", nSPSLen, nPPSLen, (int)sizeof(m_pSPSPPS));
			return;
		}
		m_nSPSPPSLen = nSPSLen + nPPSLen;
	memcpy(m_pSPSPPS, pSPSBuf, nSPSLen);
	memcpy(m_pSPSPPS + nSPSLen, pPPSBuf, nPPSLen);

	//sem_post(&m_sConnect);

	return;
}

	void RTSPPusherImpl::setSPSPPS_EX(char* pSPSPPSBuf,int nLen)
	{
		if (nLen < 0 || nLen > (int)sizeof(m_pSPSPPS))
		{
			printf("setSPSPPS_EX invalid size:%d max:%d\n", nLen, (int)sizeof(m_pSPSPPS));
			return;
		}
		m_nSPSPPSLen = nLen;
	memcpy(m_pSPSPPS, pSPSPPSBuf, nLen);

	//sem_post(&m_sConnect);
}

int RTSPPusherImpl::pushVideo(char* pBuf, int nLen, bool bKey, unsigned long long nTimeStamp)
{
	if (!m_mutexInited || pBuf == NULL || nLen <= 0)
		return -1;

	pthread_mutex_lock(&m_Lock);
	if (!m_bInited || outputContext == NULL || videoStream == NULL)
	{
		pthread_mutex_unlock(&m_Lock);
		return -1;
	}

	if (m_bNeedIframe && !bKey)
	{
		pthread_mutex_unlock(&m_Lock);
		return 0;
	}
	else
		m_bNeedIframe = false;

	if (m_lVTimeStamp == 0)
		m_lVTimeStamp = nTimeStamp;

	long long nRealTimeStamp = nTimeStamp - m_lVTimeStamp;
	if (nRealTimeStamp < 0)
	{
		m_lVTimeStamp = nTimeStamp;
		nRealTimeStamp = 0;
	}

	AVPacket packet;
	av_init_packet(&packet);
	packet.stream_index = videoStream->id;
	packet.data = reinterpret_cast<uint8_t*>(pBuf);
	packet.size = nLen;
	packet.flags = bKey ? AV_PKT_FLAG_KEY : 0;

	AVRational srcTimeBase = { 1, 1000000 };
	packet.pts = av_rescale_q(nRealTimeStamp, srcTimeBase, videoStream->time_base);
	packet.dts = packet.pts;
	AVRational frameTimeBase = { 1, m_nfps };
	packet.duration = av_rescale_q(1, frameTimeBase, videoStream->time_base);
	//printf("key: %d, video pts1: %lld, nRealTimeStamp: %lld, num: %d, den: %d\n", bKey, packet.pts, nRealTimeStamp, videoStream->time_base.num, videoStream->time_base.den);

	struct timeval write_start;
	struct timeval write_end;
	gettimeofday(&write_start, NULL);
	int nRet = av_interleaved_write_frame(outputContext, &packet);
	gettimeofday(&write_end, NULL);
	pthread_mutex_unlock(&m_Lock);
	long long write_ms = (write_end.tv_sec - write_start.tv_sec) * 1000LL +
		(write_end.tv_usec - write_start.tv_usec) / 1000LL;
	if (write_ms > 20)
	{
		printf("rtsp pusher slow write: %lld ms, fps:%d, url:%s, datalen:%d, key:%d\n",
			write_ms, m_nfps, m_sUrl, nLen, bKey);
	}

	av_packet_unref(&packet);
	if (nRet < 0)
	{
		m_nPushFrameFailCnt ++;
		char buf[1024] = { 0 };
		av_strerror(nRet, buf, sizeof(buf) - 1);
		std::cout << "av_write_frame failed.  " << buf << "  ,datalen:" << nLen << ",key:" << bKey << ",url:" << m_sUrl << ",push failed count:" << m_nPushFrameFailCnt <<std::endl;

		if (m_nPushFrameFailCnt >= 100)
		{
			printf("reconnet server:%s\n",m_sUrl);
			close();
			sem_post(&m_sConnect);
		}
	}
	else
	{
		if (m_nPushFrameFailCnt > 0)
		{
			m_nPushFrameFailCnt = 0;
		}
	}


	return 0;
}

void RTSPPusherImpl::close()
{
	m_bInited = false;

	if (!m_mutexInited)
	{
		outputContext = NULL;
		videoCodecContext = NULL;
		return;
	}

	pthread_mutex_lock(&m_Lock);
	if (NULL != outputContext)
	{
		// Write trailer to the output file
		av_write_trailer(outputContext);

		// Free the output context and associated resources
		avformat_free_context(outputContext);
		outputContext = NULL;
	}

	if (videoCodecContext != NULL)
	{
		avcodec_free_context(&videoCodecContext);
		videoCodecContext = NULL;
	}
	pthread_mutex_unlock(&m_Lock);
}

int  RTSPPusherImpl::deinit()
{
	if (m_hConnectThread != 0)
	{
		m_start_reconnect = false;
		sem_post(&m_sConnect);
		pthread_join(m_hConnectThread, NULL);
		m_hConnectThread = 0;
	}

	return 0;
}

RTSPPusherImpl::~RTSPPusherImpl()
{
	close();
	deinit();

	if (m_semInited)
	{
		sem_destroy(&m_sConnect);
		m_semInited = false;
	}
	if (m_mutexInited)
	{
		pthread_mutex_destroy(&m_Lock);
		m_mutexInited = false;
	}
}

void RTSPPusherImpl::_CloseRtspPusher()
{
	if (outputContext)
	{
		avformat_free_context(outputContext);
		outputContext = nullptr;
	}

	if (videoCodecContext != NULL)
	{
		avcodec_free_context(&videoCodecContext);
		videoCodecContext = NULL;
	}

}

void* RTSPPusherImpl::ReconnectThread(void* pParam)
{
	RTSPPusherImpl* pThis = (RTSPPusherImpl*)pParam;
	pThis->_DoReconnect();
	return NULL;
}
void RTSPPusherImpl::_DoReconnect()
{
	while (m_start_reconnect)
	{
		sem_wait(&m_sConnect);
		if (!m_start_reconnect)
		{
			close();
			return;
		}

		printf("rtsp pusher reconnecting, url: %s\n", m_sUrl);
		close();
		if (0 != open())
		{
			sleep(5);
		}
		else
		{
			printf("rtsp pusher reconnect success, url: %s\n", m_sUrl);
		}
	}
}
