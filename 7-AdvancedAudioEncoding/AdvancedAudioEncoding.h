#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libswresample/swresample.h"
#include <libavutil/opt.h>
}

class AdvancedAudioEncoding
{
public:
	AdvancedAudioEncoding();
	~AdvancedAudioEncoding();
	bool init();
	bool initCodecContext();
	bool readFrameProc(const char *input, const char *output);

private:
	AVCodecContext  *mCodecCtx;
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
};

