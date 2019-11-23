#pragma once

#include <string>
#include "FFMpegHeader.h"
#include "CustomAVFilter.hpp"
#include "AV.hpp"
#include "AudioResample.hpp"
#include<iostream>
#include<stdio.h>


class TransCode:public AV {
private:
	std::string InputFilePath = "";
	std::string OutputFilePath = "";

	AVFormatContext* InputFmtCtx = NULL;
	AVFormatContext* OutputFmtCtx = NULL;

	CustomAVFilter Filter;
	AudioResample AudioResample;
	StreamContext *StreamCtx = NULL;

	// 用于重采样形成1024个采样点
	AVAudioFifo *AudioBuffer = NULL;
	// 最近一次记录的音频包的pts大小(依次累加)
	int64_t LastAudioPacketPts = 0; 
	// 最近一次写到OutputFmtCtx的视频包的pts大小(依次累加)
	int64_t LastWriteVideoPacketPts = 0;

	bool& Exit;
	bool IsCanceled = false;

	int OpenInputFile() {
		int ret = 0;
		if ((ret = avformat_open_input(&InputFmtCtx, this->InputFilePath.c_str(), NULL, NULL)) < 0) {
			printf("Cannot open input file[%s]\n", this->InputFilePath.c_str());
			return ret;
		}

		if ((ret = avformat_find_stream_info(InputFmtCtx, NULL)) < 0) {
			printf("Cannot find stream information[%s]\n", this->InputFilePath.c_str());
			return ret;
		}

		StreamCtx = (StreamContext*)av_mallocz_array(InputFmtCtx->nb_streams, sizeof(*StreamCtx));
		if (!StreamCtx) {
			printf("mallocz array fail![%s]\n", this->InputFilePath.c_str());
			return AVERROR(ENOMEM);
		}

		int i = 0;
		for (; i < InputFmtCtx->nb_streams; i++) {
			StreamCtx[i].DecoderCtx = NULL;
			StreamCtx[i].EncoderCtx = NULL;
			StreamCtx[i].Enabled = false;
		}

		VideoStreamIndex = av_find_best_stream(InputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		AudioStreamIndex = av_find_best_stream(InputFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

		if ((VideoStreamIndex < 0 && AudioStreamIndex < 0) ||
			(VideoStreamIndex >= InputFmtCtx->nb_streams) || 
			(AudioStreamIndex >= InputFmtCtx->nb_streams)) {
			printf("VideoStreamIndex[%d]AudioStreamIndex[%d]InputFilePath[%s]\n",
				this->VideoStreamIndex, this->AudioStreamIndex, this->InputFilePath.c_str());
			return AVERROR(ENOMEM);
		}

		if (AudioStreamIndex >= 0) {
			StreamCtx[AudioStreamIndex].Enabled = true;
		}
		
		for (int i = 0; i < InputFmtCtx->nb_streams; i++) {
			AVStream *stream = InputFmtCtx->streams[i];
			AVCodec *decoder = NULL;
			AVCodecID codecId;
			AVCodecContext *codecCtx;

			decoder = avcodec_find_decoder(stream->codecpar->codec_id);
			if (!decoder) {
				printf("Failed to find decoder for stream[%d][%s]\n", i, this->InputFilePath.c_str());
				return AVERROR_DECODER_NOT_FOUND;
			}

			codecCtx = avcodec_alloc_context3(decoder);
			if (!codecCtx) {
				printf("Failed to allocate the decoder context for stream[%d][%s]\n", i, this->InputFilePath.c_str());
				return AVERROR(ENOMEM);
			}

			ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
			if (ret < 0) {
				printf("Failed to copy decoder parameters to input decoder context[%d][%s]\n", i, this->InputFilePath.c_str());
				return ret;
			}

			if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
				codecCtx->framerate = av_guess_frame_rate(InputFmtCtx, stream, NULL);
			}

			ret = avcodec_open2(codecCtx, decoder, NULL);
			if (ret < 0) {
				printf("Failed to open decoder for stream[%d][%s]\n", i, this->InputFilePath.c_str());
				return ret;
			}
			StreamCtx[i].DecoderCtx = codecCtx;
		}
		return 0;
	}

	int OpenOutputFile() {
		AVStream *outputStream;
		AVStream *inputStream;
		AVCodecContext *decoderCtx, *encoderCtx;
		AVCodec *encoder;
		AVCodecID codecId;
		int ret;

		avformat_alloc_output_context2(&OutputFmtCtx, NULL, NULL, this->OutputFilePath.c_str());
		if (!OutputFmtCtx) {
			printf("Could not create output context[%s]\n", this->OutputFilePath.c_str());
			return AVERROR_UNKNOWN;
		}

		for (int i = 0; i < InputFmtCtx->nb_streams; i++) {
			outputStream = avformat_new_stream(OutputFmtCtx, NULL);
			if (!outputStream) {
				printf("Failed allocating output stream[%s]\n", this->OutputFilePath.c_str());
				return AVERROR_UNKNOWN;
			}

			inputStream = InputFmtCtx->streams[i];
			decoderCtx = StreamCtx[i].DecoderCtx;

			if (decoderCtx->codec_type == AVMEDIA_TYPE_VIDEO
				|| decoderCtx->codec_type == AVMEDIA_TYPE_AUDIO) {

				codecId = AV_CODEC_ID_AAC;
				if (decoderCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
					codecId = decoderCtx->codec_id;
				}

				encoder = avcodec_find_encoder(codecId);
				if (!encoder) {
					printf("Necessary encoder not found[%d][%s]\n", i, this->OutputFilePath.c_str());
					return AVERROR_INVALIDDATA;
				}

				encoderCtx = avcodec_alloc_context3(encoder);
				if (!encoderCtx) {
					printf("Failed to allocate the encoder context[%d][%s]\n", i, this->OutputFilePath.c_str());
					return AVERROR(ENOMEM);
				}

				if (encoderCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
					encoderCtx->height = decoderCtx->height;
					encoderCtx->width = decoderCtx->width;
					encoderCtx->sample_aspect_ratio = decoderCtx->sample_aspect_ratio;
					if (encoder->pix_fmts) {
						encoderCtx->pix_fmt = encoder->pix_fmts[0];
					}
					else {
						encoderCtx->pix_fmt = decoderCtx->pix_fmt;
					}
					encoderCtx->time_base = av_inv_q(decoderCtx->framerate);
				}
				else {
					encoderCtx->bit_rate = 64000;
					encoderCtx->codec_type = AVMediaType::AVMEDIA_TYPE_AUDIO;
					encoderCtx->codec_id = AV_CODEC_ID_AAC;
					encoderCtx->channel_layout = AV_CH_LAYOUT_STEREO;
					encoderCtx->channels = av_get_channel_layout_nb_channels(encoderCtx->channel_layout);
					encoderCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
					encoderCtx->sample_rate = 44100;
					encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
					encoderCtx->frame_size = 1024;
					encoderCtx->thread_count = 1;
				}

				if (OutputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
					encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
				}

				ret = avcodec_open2(encoderCtx, encoder, NULL);
				if (ret < 0) {
					printf("Cannot open video encoder for stream[%d][%s]", i, this->OutputFilePath.c_str());
					return ret;
				}
				ret = avcodec_parameters_from_context(outputStream->codecpar, encoderCtx);
				if (ret < 0) {
					printf("Failed to copy encoder parameters to output stream[%d][%s]", i, this->OutputFilePath.c_str());
					return ret;
				}
				outputStream->time_base = encoderCtx->time_base;
				StreamCtx[i].EncoderCtx = encoderCtx;
			}
			else if (decoderCtx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
				printf("Elementary stream[%d] is of unknown type, cannot proceed[%s]", i, this->OutputFilePath.c_str());
				return AVERROR_INVALIDDATA;
			}
			else {
				ret = avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
				if (ret < 0) {
					printf("Copying parameters for stream[%d] failed[%s]", i, this->OutputFilePath.c_str());
					return ret;
				}
				outputStream->time_base = inputStream->time_base;
			}
		}

		if (!(OutputFmtCtx->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&OutputFmtCtx->pb, OutputFilePath.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				printf("Could not open output file[%s]", this->OutputFilePath.c_str());
				return ret;
			}
		}

		ret = avformat_write_header(OutputFmtCtx, NULL);
		if (ret < 0) {
			printf("Error occurred when opening output file");
			return ret;
		}
		return 0;
	}

	/**
	 * 初始化过滤图
	 */
	bool InitFiltergraph() {
		return Filter.InitFilters(InputFmtCtx, StreamCtx) >= 0;
	}

	int FilterEncodeWriteFrame(AVFrame *frame, unsigned int streamIndex) {
		AVFrame *filterFrame;

		int ret = Filter.Buffersrc_add_frame(frame, streamIndex);
		if (ret < 0) {
			printf("Error while feeding the filtergraph");
			return ret;
		}

		while (1) {
			if (Exit) {
				SetCancel(true);
				break;
			}

			filterFrame = av_frame_alloc();
			if (!filterFrame) {
				ret = AVERROR(ENOMEM);
				break;
			}

			ret = Filter.Buffersink_get_frame(filterFrame, streamIndex);
			if (ret < 0) {
				av_frame_free(&filterFrame);
				break;
			}
			filterFrame->pict_type = AV_PICTURE_TYPE_NONE;

			if (AudioResample.AddSamplesToFifo(AudioBuffer, (uint8_t**)filterFrame->data, filterFrame->nb_samples) < 0) {
				av_frame_free(&filterFrame);
				break;
			}

			while (av_audio_fifo_size(AudioBuffer) >= filterFrame->nb_samples) {
				if ((ret = LoadEncodeAndWrite(streamIndex)) < 0) {
					printf("LoadEncodeAndWrite fail,ret[%d]in[%s]out[%s]",
						ret, this->InputFilePath.c_str(), this->OutputFilePath.c_str());
					break;
				}
			}
			av_frame_free(&filterFrame);
		}
		return ret;
	}

	int LoadEncodeAndWrite(unsigned int streamIndex) {
		int ret;
		AVFrame *outputFrame = NULL;
		const int frameSize = FFMIN(av_audio_fifo_size(AudioBuffer),
			StreamCtx[AudioStreamIndex].EncoderCtx->frame_size);

		if (ret = InitOutputFrame(&outputFrame, StreamCtx[AudioStreamIndex].EncoderCtx, frameSize)) {
			return ret;
		}

		if (ret = av_audio_fifo_read(AudioBuffer, (void**)outputFrame->data, frameSize) < frameSize) {
			av_frame_free(&outputFrame);
			return ret;
		}

		ret = EncodeAudioFrame(outputFrame, streamIndex);
		av_frame_free(&outputFrame);
		return ret;
	}

	int EncodeAudioFrame(AVFrame *frame, unsigned int streamIndex) {
		AVPacket outputPacket;
		InitPacket(&outputPacket);

		int ret = avcodec_send_frame(StreamCtx[streamIndex].EncoderCtx, frame);

		if (ret < 0) {
			printf("send frame error!");
			return ret;
		}

		ret = avcodec_receive_packet(StreamCtx[streamIndex].EncoderCtx, &outputPacket);
		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
			return ret;
		} else if (ret < 0) {
			printf("Error encoding audio frame");
			return ret;
		}

		int duration = 1024;
		if (frame != NULL) {
			duration = frame->nb_samples;
		}

		outputPacket.pts = LastAudioPacketPts + duration;
		outputPacket.dts = outputPacket.pts;
		outputPacket.stream_index = streamIndex;
		LastAudioPacketPts = outputPacket.pts;

		try {
			ret = av_interleaved_write_frame(OutputFmtCtx, &outputPacket);
		} catch (...) {
			printf("write frame crash!");
		}
		
		if (ret < 0) {
			printf("write frame error!");
		}
		av_packet_unref(&outputPacket);
		return ret;
	}

	int FlushEncoder(unsigned int streamIndex) {
		int ret;
		if (!(StreamCtx[streamIndex].EncoderCtx->codec->capabilities &
			AV_CODEC_CAP_DELAY)) {
			return 0;
		}

		while (1) {
			if (Exit) {
				SetCancel(true);
				break;
			}

			ret = EncodeAudioFrame(NULL, streamIndex);
			if (ret < 0) {
				break;
			}
		}
		return ret;
	}

	/**
	 * 释放资源
	 */
	int TranscodeEnd(int ret, AVFrame **frame, AVPacket &packet) {
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

		if (AudioBuffer) {
			av_audio_fifo_free(AudioBuffer);
		}
	}

	void SetCancel(bool isCanceled) {
		this->IsCanceled = isCanceled;
	}

public:
	TransCode(bool& exit) :Exit(exit) {}

	/**
	 * 转码初始化
	 */
	bool Initial(const char* inputFilePath, const char* outputFilePath) {
		this->InputFilePath = inputFilePath;
		this->OutputFilePath = outputFilePath;
		return true;
	}

	/**
	 * 开始转码
	 */
	int TranscodeStep() {
		int ret = 0;
		AVPacket packet;
		AVFrame *frame = NULL;
		enum AVMediaType type;

		av_init_packet(&packet);

		if ((ret = OpenInputFile() < 0)) {
			return TranscodeEnd(ret, &frame, packet);
		}

		if ((ret = OpenOutputFile()) < 0) {
			return TranscodeEnd(ret, &frame, packet);
		}

		if (!InitFiltergraph()) {
			return TranscodeEnd(ret, &frame, packet);
		}

		if (AudioResample.InitFifo(AudioStreamIndex, StreamCtx, &AudioBuffer) < 0) {
			return TranscodeEnd(ret, &frame, packet);
		}

		// 从InputFmtCtx中取出的第一包的流索引(音频or视频)
		int64_t firstPacketStreamIndex = -1;

		// 上一次调整过的视频包的pts
		int64_t lastAdjustVideoPacketPts = 0; 
		// 当前从InputFmtCtx中取出的视频包的pts
		int64_t currVideoPacketPts = 0;
		// 上一次从InputFmtCtx中取出的视频包的pts
		int64_t lastVideoPacketPts = 0; 
		// 从InputFmtCtx中取出的视频包的时长刻度（理论值）
		int64_t videoPacektDuration = (double)1 / (av_q2d(InputFmtCtx->streams[VideoStreamIndex]->time_base) *
			av_q2d(InputFmtCtx->streams[VideoStreamIndex]->r_frame_rate));

		while (1) {
			if (Exit) { // 退出直接取消
				SetCancel(true);
				printf("convert canceled!InputFilePath[%s]OutputFilePath[%s]", 
					this->InputFilePath.c_str(), this->OutputFilePath.c_str());
				return TranscodeEnd(ret, &frame, packet);
			}

			if ((ret = av_read_frame(InputFmtCtx, &packet)) < 0) { 
				if (ret == AVERROR_EOF) {
					ret = 0;
					break;
				}
			}

			if (packet.pts <= 0) {
				printf("packet.pts[%d] is litter than 0", packet.pts);
				av_packet_unref(&packet);
				continue;
			}

			if (firstPacketStreamIndex < 0) {
				firstPacketStreamIndex = packet.stream_index;
			}

			type = InputFmtCtx->streams[packet.stream_index]->codecpar->codec_type;

			if (type == AVMEDIA_TYPE_AUDIO) {
				continue;
			}

			if (type == AVMEDIA_TYPE_AUDIO) {
				/**
				 * 1、从InputFmtCtx从第一次取出的包的流索引是否是音频流索引，来确定是否需要补音，是：不需要，反之，需要
				 * 2、哪怕满足补音条件(即1)，但是补音过了(LastAudioPacketPts),就不需要补音了
				 */
				if (LastAudioPacketPts <= 0) { // 补静音
					if (firstPacketStreamIndex == VideoStreamIndex) {
						// 补音数量的计算，改进
						int64_t fillSilenceVoiceCount = av_rescale_q_rnd(LastWriteVideoPacketPts,
							OutputFmtCtx->streams[VideoStreamIndex]->time_base,
							OutputFmtCtx->streams[AudioStreamIndex]->time_base,
							(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)) / 1024;

						if (fillSilenceVoiceCount > 0) {
							unsigned char silenceVoiceData[6] = { 0x21, 0x10, 0x04, 0x60, 0x8c, 0x1c };
							AVPacket* silenceVoicePacket = av_packet_alloc();
							if (!silenceVoicePacket) {
								break;
							}
							InitPacket(silenceVoicePacket);

							for (; fillSilenceVoiceCount > 0; fillSilenceVoiceCount--) { // 填充静默音
								silenceVoicePacket->data = silenceVoiceData;
								silenceVoicePacket->size = sizeof(silenceVoiceData);
								silenceVoicePacket->stream_index = AudioStreamIndex;
								silenceVoicePacket->pts = LastAudioPacketPts + StreamCtx[packet.stream_index].EncoderCtx->frame_size;
								silenceVoicePacket->dts = silenceVoicePacket->pts;
								LastAudioPacketPts = silenceVoicePacket->pts;

								ret = av_interleaved_write_frame(OutputFmtCtx, silenceVoicePacket);
								if (ret < 0) {
									break;
								}
								av_packet_unref(silenceVoicePacket);
							}
							av_packet_free(&silenceVoicePacket);
						}
					}
				}

				frame = av_frame_alloc();
				if (!frame) {
					ret = AVERROR(ENOMEM);
					break;
				}

				int ret = avcodec_send_packet(StreamCtx[packet.stream_index].DecoderCtx, &packet);
				if (ret < 0) {
					av_frame_free(&frame);
					frame = NULL;
					printf("Error while sending a packet to the decoder");
					break;
				}

				while (1) {
					if (Exit) {
						SetCancel(true);
						printf("convert canceled!InputFilePath[%s]OutputFilePath[%s]",
							this->InputFilePath.c_str(), this->OutputFilePath.c_str());
						return TranscodeEnd(ret, &frame, packet);
					}

					ret = avcodec_receive_frame(StreamCtx[packet.stream_index].DecoderCtx, frame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
						av_frame_free(&frame);
						frame = NULL;
						break;
					} else if (ret < 0) {
						printf("Error while receiving a frame from the decoder!");
						return TranscodeEnd(ret, &frame, packet);
					}

					if (frame->pts == AV_NOPTS_VALUE) {
						frame->pts = frame->best_effort_timestamp;
					}

					ret = FilterEncodeWriteFrame(frame, packet.stream_index);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
						ret = 0;
					}

					if (ret < 0) {
						printf("fliter encoder write frame error!");
						return TranscodeEnd(ret, &frame, packet);
					}
					av_frame_unref(frame);
				}
			} else {
				if (!CheckPacket(packet)) {
					cout << "checkPacket fail!";
					av_packet_unref(&packet);
					continue;
				}

				currVideoPacketPts = packet.pts;
				int64_t videoPacketPtsInterval = packet.pts - lastVideoPacketPts;

				if (videoPacketPtsInterval == 0) {
					// 当前包的pts等于上一包的pts，将当前包抛弃
					av_packet_unref(&packet);
					continue;
				} else if (videoPacketPtsInterval < 0 || (videoPacketPtsInterval > videoPacektDuration * 6)) {
					packet.duration = videoPacektDuration;
				} else {
					packet.duration = videoPacketPtsInterval;
				}
				packet.pts = packet.dts = lastAdjustVideoPacketPts + packet.duration;

				lastVideoPacketPts = currVideoPacketPts;
				lastAdjustVideoPacketPts = packet.pts;
				av_packet_rescale_ts(&packet, InputFmtCtx->streams[packet.stream_index]->time_base, OutputFmtCtx->streams[packet.stream_index]->time_base);
				LastWriteVideoPacketPts = packet.pts;

				try {
					ret = av_interleaved_write_frame(OutputFmtCtx, &packet);
				} catch (...) {
					printf("write frame crash!");
					return TranscodeEnd(ret, &frame, packet);
				}
				
				if (ret < 0) {
					printf("write frame error!");
					return TranscodeEnd(ret, &frame, packet);
				}
			}
			av_packet_unref(&packet);
		}

		printf("Transcode is end,flush will start,inputFilePath[%s]outputFilePath[%s]\n",
			this->InputFilePath.c_str(), this->OutputFilePath.c_str());

		/**
		 * for块刷新缓冲区,刷新失败也算成功
		 */
		for (int i = 0; i < InputFmtCtx->nb_streams; i++) {
			if (StreamCtx[i].EncoderCtx->codec_type != AVMEDIA_TYPE_AUDIO) {
				continue;
			}

			while (1) {
				ret = FilterEncodeWriteFrame(NULL, i);
				if (ret < 0) {
					if (ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) {
						printf("Flushing FilterEncodeWriteFrame failed!\n");
					}
					ret = 0;
					break;
				}
			}

			while (1) {
				ret = FlushEncoder(i);
				if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
					ret = 0;
					break;
				}

				if (ret < 0) {
					ret = 0;
					printf("Flushing encoder failed\n");
					break;
				}
			}
			
		}
		av_write_trailer(OutputFmtCtx);
		return TranscodeEnd(ret, &frame, packet);
	}

	bool GetCancel() {
		return this->IsCanceled;
	}
};