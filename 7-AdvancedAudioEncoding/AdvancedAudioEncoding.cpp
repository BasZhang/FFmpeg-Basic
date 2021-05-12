#include "AdvancedAudioEncoding.h"

AdvancedAudioEncoding::AdvancedAudioEncoding()
	:mCodecCtx(NULL)
{
}


AdvancedAudioEncoding::~AdvancedAudioEncoding()
{
	avcodec_free_context(&mCodecCtx);
}

bool AdvancedAudioEncoding::init()
{
	av_register_all();
	avcodec_register_all();
	return false;
}

bool AdvancedAudioEncoding::initCodecContext()
{
	const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!enc) {
		printf("Failed to find codec\n");
	}

	mCodecCtx = avcodec_alloc_context3(enc);
	if (!mCodecCtx) {
		printf("Failed to allocate the codec context\n");
		return true;
	}

	//mCodecCtx->bit_rate = 320000;
	mCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
	mCodecCtx->sample_rate = 48000;
	mCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	mCodecCtx->channels = 2;
	mCodecCtx->profile = FF_PROFILE_AAC_SSR;
	mCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    pFormatCtx = avformat_alloc_context();
    fmt = av_guess_format(NULL, ".aac", NULL);
    pFormatCtx->oformat = fmt;

	if (avcodec_open2(mCodecCtx, enc, NULL) < 0) {
		printf("Failed to open encodec\n");
		return true;
	}
	return false;
}

bool AdvancedAudioEncoding::readFrameProc(const char * input, const char * output)
{
	FILE *pcmFd = fopen(input, "rb");
	FILE *outFd = fopen(output, "wb");
	SwrContext *swr;

	if (!outFd || !pcmFd) {
		fprintf(stderr, "Could not open file\n");
		return true;
	}

	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		printf("Failed to allocate video frame\n");
		return true;
	}

	frame->format = mCodecCtx->sample_fmt;
	frame->nb_samples = mCodecCtx->frame_size;
	frame->channel_layout = mCodecCtx->channel_layout;

	swr = swr_alloc();
	av_opt_set_int(swr, "in_channel_layout", mCodecCtx->channel_layout, 0);
	av_opt_set_int(swr, "out_channel_layout", mCodecCtx->channel_layout, 0);
	av_opt_set_int(swr, "in_sample_rate", mCodecCtx->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate", mCodecCtx->sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	swr_init(swr);

	if (av_frame_get_buffer(frame, 0) < 0) {
		printf("Failed to allocate the video frame data\n");
		return true;
	}
	//av_samples_alloc_array_and_samples(&src_data, &src_linesize,
	//	mCodecCtx->channels,//number of audio channels
	//	mCodecCtx->frame_size,//incodecContext->frame_size, //nb_samples  number of samples per channel
	//	mCodecCtx->sample_fmt, 0);
	//
	uint8_t *outs[2];

	outs[0] = (uint8_t *)malloc(4096);
	outs[1] = (uint8_t *)malloc(4096);

	uint8_t *readBuffer = (uint8_t *)malloc(4096);

	int num = 0;
	AVPacket pkt;

	uint8_t head[7] = { 0 };
	uint8_t profile = 2;
	uint8_t freqIdx = 3;
	uint8_t chanCfg = 2;

	// read a frame every time
	while (!feof(pcmFd)) {

		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		if (av_frame_make_writable(frame)) {
			return true;
		}
		int sampleBytes = av_get_bytes_per_sample(mCodecCtx->sample_fmt);

		//Read PCM
		size_t readN = fread(readBuffer, 1, 4096, pcmFd);
		if (readN <= 0) {
			printf("Failed to read raw data! \n");
			goto FIN;
		}
		int count = swr_convert(swr, outs, 4096, (const uint8_t**)&readBuffer, 1024);
		frame->data[0] = outs[0];
		frame->data[1] = outs[1];

		FIN:
		// encode audio
		avcodec_send_frame(mCodecCtx, frame);
		int ret = avcodec_receive_packet(mCodecCtx, &pkt);

		if (!ret) {
			printf("Write frame %3d (size=%5d)\n", num++, pkt.size);
			int size = 7 + pkt.size;
			head[0] = 0xFF;
			head[1] = 0xF1;
			head[2] = (uint8_t)(((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));
			head[3] = (uint8_t)(((chanCfg & 3) << 6) + (7 + size >> 11));
			head[4] = (uint8_t)((size & 0x7FF) >> 3);
			head[5] = (uint8_t)(((size & 7) << 5) + 0x1F);
			head[6] = 0xFC;
			fwrite(head, 1, 7, outFd);
			fwrite(pkt.data, 1, pkt.size, outFd);
			av_packet_unref(&pkt);
		}
	}

	printf("------------- get delayed data --------------------\n");

	// get delayed data
	for (;;) {
		avcodec_send_frame(mCodecCtx, NULL);
		int ret = avcodec_receive_packet(mCodecCtx, &pkt);
		if (ret == 0) {
			printf("Write frame %3d (size=%5d)\n", num++, pkt.size);
			int size = 7 + pkt.size;
			head[0] = 0xFF;
			head[1] = 0xF1;
			head[2] = (uint8_t)(((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));
			head[3] = (uint8_t)(((chanCfg & 3) << 6) + (7 + size >> 11));
			head[4] = (uint8_t)((size & 0x7FF) >> 3);
			head[5] = (uint8_t)(((size & 7) << 5) + 0x1F);
			head[6] = 0xFC;
			fwrite(head, 1, 7, outFd);
			fwrite(pkt.data, 1, pkt.size, outFd);
			av_packet_unref(&pkt);
		}
		else if (ret == AVERROR_EOF) {
			printf("Write frame complete\n");
			break;
		}
		else {
			printf("Error encoding frame\n");
			break;
		}

	}

	fclose(outFd);
	fclose(pcmFd);
	av_frame_free(&frame);
	swr_free(&swr);

	return false;
}

