#pragma once

#include "FFMpegHeader.h"
#include "AVFilter.hpp"
#include "AV.hpp"
#include "AudioResample.hpp"

class VideoCoverImage:public AV {
private:
	std::string InputFilePath = "";
	std::string OutputFilePath = "";

	AVFormatContext* InputFmtCtx = NULL;
	AVFormatContext* OutputFmtCtx = NULL;

	int Width = -1;
	int Height = -1;

	HikAVFilter Filter;
	StreamContext *StreamCtx = NULL;

	bool FindVideoStream(AVStream **stream) {
		for (int i = 0; i < InputFmtCtx->nb_streams; i++) {
			AVStream *inputStream = InputFmtCtx->streams[i];
			if (inputStream->index == VideoStreamIndex) {
				*stream = inputStream;
				break;
			}

			if (i == InputFmtCtx->nb_streams) {
				return false;
			}
		}
		return true;
	}

	int OpenInputFile() {
		int ret = 0;

		if ((ret = avformat_open_input(&InputFmtCtx, this->InputFilePath.c_str(), NULL, NULL)) < 0) {
			printf("Cannot open input file[%s]", this->InputFilePath.c_str());
			return ret;
		}

		if ((ret = avformat_find_stream_info(InputFmtCtx, NULL)) < 0) {
			printf("Cannot find stream information[%s]", this->InputFilePath.c_str());
			return ret;
		}

		StreamCtx = (StreamContext*)av_mallocz_array(InputFmtCtx->nb_streams, sizeof(*StreamCtx));
		if (!StreamCtx) {
			return AVERROR(ENOMEM);
		}

		int i = 0;
		for (; i < InputFmtCtx->nb_streams; i++) {
			StreamCtx[i].DecoderCtx = NULL;
			StreamCtx[i].EncoderCtx = NULL;
			StreamCtx[i].Enabled = false;
		}

		VideoStreamIndex = av_find_best_stream(InputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		if (VideoStreamIndex < 0 || VideoStreamIndex >= InputFmtCtx->nb_streams) {
			printf("VideoStreamIndex[%d]InputPath[%s]", VideoStreamIndex, this->InputFilePath.c_str());
			return AVERROR(ENOMEM);
		}

		StreamCtx[VideoStreamIndex].Enabled = true;
		AVStream *stream = NULL;

		if (!FindVideoStream(&stream)) {
			printf("Cannot find video stream[%d][%s]",VideoStreamIndex, this->InputFilePath.c_str());
			return -1;
		}

		AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
		if (!decoder) {
			printf("Failed to find decoder for stream[%d][%s]",VideoStreamIndex, this->InputFilePath.c_str());
			return AVERROR_DECODER_NOT_FOUND;
		}

		AVCodecContext *codecCtx = avcodec_alloc_context3(decoder);
		if (!codecCtx) {
			printf("Failed to allocate the decoder context for stream[%d][%s]", VideoStreamIndex, this->InputFilePath.c_str());
			return AVERROR(ENOMEM);
		}

		ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
		if (ret < 0) {
			printf("Failed to copy decoder parameters to input decoder context for stream[%d][%s]",
				VideoStreamIndex, this->InputFilePath.c_str());
			return ret;
		}

		codecCtx->framerate = av_guess_frame_rate(InputFmtCtx, stream, NULL);

		ret = avcodec_open2(codecCtx, decoder, NULL);
		if (ret < 0) {
			printf("Failed to open decoder for stream[%d][%s]", VideoStreamIndex, this->InputFilePath.c_str());
			return ret;
		}

		StreamCtx[VideoStreamIndex].DecoderCtx = codecCtx;
		return 0;
	}

	int OpenOutputFile() {
		avformat_alloc_output_context2(&OutputFmtCtx, NULL, NULL, this->OutputFilePath.c_str());
		if (!OutputFmtCtx) {
			printf("Could not create output context for stream[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return AVERROR_UNKNOWN;
		}

		AVStream *outputStream = avformat_new_stream(OutputFmtCtx, NULL);
		if (!outputStream) {
			printf("Failed allocating output stream[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return AVERROR_UNKNOWN;
		}

		AVCodecContext *decoderCtx = StreamCtx[VideoStreamIndex].DecoderCtx;
		if (!decoderCtx) {
			printf("Failed to find the decoder context[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return AVERROR(ENOMEM);
		}

		AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
		if (!encoder) {
			printf("Necessary encoder not found[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return AVERROR_INVALIDDATA;
		}

		AVCodecContext *encoderCtx = avcodec_alloc_context3(encoder);
		if (!encoderCtx) {
			printf("Failed to allocate the encoder context[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return AVERROR(ENOMEM);
		}

		encoderCtx->height = this->Height;
		encoderCtx->width = this->Width;
		encoderCtx->sample_aspect_ratio = decoderCtx->sample_aspect_ratio;
		encoderCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
		encoderCtx->time_base = av_inv_q(decoderCtx->framerate);

		if (OutputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
			encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}

		int ret = avcodec_open2(encoderCtx, encoder, NULL);
		if (ret < 0) {
			printf("Cannot open video encoder for stream[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return ret;
		}

		ret = avcodec_parameters_from_context(outputStream->codecpar, encoderCtx);
		if (ret < 0) {
			printf("Failed to copy encoder parameters to output stream[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return ret;
		}
		outputStream->time_base = encoderCtx->time_base;
		outputStream->index = VideoStreamIndex;
		outputStream->codecpar->codec_tag = 0;

		StreamCtx[VideoStreamIndex].EncoderCtx = encoderCtx;

		if (!(OutputFmtCtx->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&OutputFmtCtx->pb, OutputFilePath.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				printf("Could not open output file[%d][%s]",
					VideoStreamIndex, this->OutputFilePath.c_str());
				return ret;
			}
		}

		ret = avformat_write_header(OutputFmtCtx, NULL);
		if (ret < 0) {
			printf("Error occurred when opening output file[%d][%s]",
				VideoStreamIndex, this->OutputFilePath.c_str());
			return ret;
		}
		return 0;
	}

	/**
	 * 初始化过滤图
	 */
	bool InitFiltergraph() {
		int width = StreamCtx[VideoStreamIndex].DecoderCtx->width;
		int height = StreamCtx[VideoStreamIndex].DecoderCtx->height;
		int pix_fmt = StreamCtx[VideoStreamIndex].DecoderCtx->pix_fmt;

		AVRational timeBase = InputFmtCtx->streams[VideoStreamIndex]->time_base;
		AVRational sampleAspectRatio = StreamCtx[VideoStreamIndex].DecoderCtx->sample_aspect_ratio;
		string args = FFMpegUtil::VideoInputFiltersArgs(width, height, pix_fmt, timeBase, sampleAspectRatio);
		string filterSpec = FFMpegUtil::VideoOutput_scale(this->Width, this->Height).c_str();
		return Filter.InitFilters(InputFmtCtx, StreamCtx, args, filterSpec) >= 0;
	}

	int FilterEncodeWriteFrame(AVFrame *frame, unsigned int streamIndex) {
		int ret;
		AVFrame *filterFrame;

		ret = Filter.Buffersrc_add_frame(frame, streamIndex);
		if (ret < 0) {
			printf("Error while feeding the filtergraph!");
			return ret;
		}

		filterFrame = av_frame_alloc();
		if (!filterFrame) {
			ret = AVERROR(ENOMEM);
			return ret;
		}

		ret = Filter.Buffersink_get_frame(filterFrame, streamIndex);
		if (ret < 0) {
			av_frame_free(&filterFrame);
			return ret;
		}
		ret = EncodeVideoFrame(filterFrame, streamIndex);
		av_frame_free(&filterFrame);
		return ret;
	}

	int EncodeVideoFrame(AVFrame *frame, unsigned int streamIndex) {
		int ret;
		AVPacket outputPacket;
		InitPacket(&outputPacket);

		ret = avcodec_send_frame(StreamCtx[streamIndex].EncoderCtx, frame);

		if (ret < 0) {
			printf("Error send video frame!");
			return ret;
		}

		ret = avcodec_receive_packet(StreamCtx[streamIndex].EncoderCtx, &outputPacket);

		if (ret < 0) {
			printf("Error receive video packet!");
			return ret;
		}

		ret = av_interleaved_write_frame(OutputFmtCtx, &outputPacket);

		if (ret < 0) {
			printf("Error write frame!");
			av_packet_unref(&outputPacket);
			return ret;
		}

		av_packet_unref(&outputPacket);
		return ret;
	}

	/**
	 * 释放资源
	 */
	int CoverImageEnd(int ret, AVFrame **frame, AVPacket &packet) {
		av_packet_unref(&packet);

		if (frame != NULL) {
			av_frame_free(frame);
			*frame = NULL;
		}

		ReleaseGlobalRes();

		if (ret < 0) {
			printf("Error occurred!\n");
		}
		return ret;
	}

	/**
	 * 释放全局资源(即整个类中都可以访问的资源,如类成员变量)
	 */
	void ReleaseGlobalRes() {
		int i = 0;
		if (NULL != StreamCtx) {
			for (; InputFmtCtx && i < InputFmtCtx->nb_streams; i++) {
				if (NULL != StreamCtx[i].DecoderCtx) {
					avcodec_free_context(&StreamCtx[i].DecoderCtx);
				}
			}

			i = 0;
			for (; OutputFmtCtx && i < OutputFmtCtx->nb_streams; i++) {
				if (NULL != StreamCtx[i].EncoderCtx) {
					avcodec_free_context(&StreamCtx[i].EncoderCtx);
				}
			}
		}

		if (NULL != InputFmtCtx) {
			Filter.EndFilter(InputFmtCtx);
		}

		if (NULL != StreamCtx) {
			av_free(StreamCtx);
		}

		if (NULL != InputFmtCtx) {
			avformat_close_input(&InputFmtCtx);
		}

		if (OutputFmtCtx) {
			if (!(OutputFmtCtx->oformat->flags & AVFMT_NOFILE)) {
				avio_closep(&OutputFmtCtx->pb);
			}

			if (!OutputFmtCtx) {
				avformat_free_context(OutputFmtCtx);
			}
		}
	}

public:
	VideoCoverImage(const char* input, const char* output, 
		int width, int height) {
		this->InputFilePath = input;
		this->OutputFilePath = output;
		this->Width = width;
		this->Height = height;
	}

	/**
	 * 截图的控制中心
	 */
	int CoverImageStep() {
		int ret = 0;
		AVPacket packet;
		AVFrame *frame = NULL;

		av_init_packet(&packet);

		if ((ret = OpenInputFile() < 0)) {
			return CoverImageEnd(ret, &frame, packet);
		}

		if ((ret = OpenOutputFile()) < 0) {
			return CoverImageEnd(ret, &frame, packet);
		}

		if (!InitFiltergraph()) {
			ret = -1;
			return CoverImageEnd(ret, &frame, packet);
		}

		while (1) {

			if ((ret = av_read_frame(InputFmtCtx, &packet)) < 0) {
				if (ret == AVERROR_EOF) {
					ret = 0;
					break;
				}
			}

			enum AVMediaType type = InputFmtCtx->streams[packet.stream_index]->codecpar->codec_type;
			if (type != AVMEDIA_TYPE_VIDEO) {
				continue;
			}

			frame = av_frame_alloc();
			if (!frame) {
				ret = AVERROR(ENOMEM);
				break;
			}

			int ret = avcodec_send_packet(StreamCtx[packet.stream_index].DecoderCtx, &packet);
			if (ret < 0) {
				printf("Error while sending a packet to the decoder\n");
				return CoverImageEnd(ret, &frame, packet);
			}

			ret = avcodec_receive_frame(StreamCtx[packet.stream_index].DecoderCtx, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				av_frame_free(&frame);
				frame = NULL;
				continue;
			} else if (ret < 0) {
				printf("Error while receiving a frame from the decoder\n");
				return CoverImageEnd(ret, &frame, packet);
			}

			ret = FilterEncodeWriteFrame(frame, packet.stream_index);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				ret = 0;
			}

			if (ret < 0) {
				return CoverImageEnd(ret, &frame, packet);
			}

			av_frame_unref(frame);
			av_packet_unref(&packet);
			break;
		}

		av_write_trailer(OutputFmtCtx);
		return CoverImageEnd(ret, &frame, packet);
	}
};