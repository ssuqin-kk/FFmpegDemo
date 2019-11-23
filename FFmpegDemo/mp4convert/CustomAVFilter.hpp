#pragma once

#include "FFMpegUtil.h"

class CustomAVFilter {
private:
	typedef struct FilteringContext {
		AVFilterContext *BuffersinkCtx = NULL;
		AVFilterContext *BuffersrcCtx = NULL;
		AVFilterGraph *FilterGraph = NULL;
	} FilteringContext;

	FilteringContext *FilterCtx = NULL;

public:
	CustomAVFilter() {
	}

	int InitFilters(AVFormatContext *inputFmtCtx, StreamContext *streamCtx, string args = "", string filterSpec = "anull") {
		unsigned int i;
		int ret = 0;
		FilterCtx = (FilteringContext*)av_malloc_array(inputFmtCtx->nb_streams, sizeof(*FilterCtx));
		if (!FilterCtx) {
			return AVERROR(ENOMEM);
		}

		for (i = 0; i < inputFmtCtx->nb_streams; i++) {
			FilterCtx[i].BuffersrcCtx = NULL;
			FilterCtx[i].BuffersinkCtx = NULL;
			FilterCtx[i].FilterGraph = NULL;
		}

		for (i = 0; i < inputFmtCtx->nb_streams; i++) {
			if (!streamCtx[i].Enabled) {
				continue;
			}
			
			if (args.empty()) {
				char cargs[512] = { 0 };
				if (streamCtx[i].DecoderCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
					FFMpegUtil::snprintf(cargs, sizeof(cargs),
						"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
						streamCtx[i].DecoderCtx->width,
						streamCtx[i].DecoderCtx->height,
						streamCtx[i].DecoderCtx->pix_fmt,
						streamCtx[i].DecoderCtx->time_base.num,
						streamCtx[i].DecoderCtx->time_base.den,
						streamCtx[i].DecoderCtx->sample_aspect_ratio.num,
						streamCtx[i].DecoderCtx->sample_aspect_ratio.den);
				} else {
					FFMpegUtil::snprintf(cargs, sizeof(cargs),
						"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
						streamCtx[i].DecoderCtx->time_base.num,
						streamCtx[i].DecoderCtx->time_base.den,
						streamCtx[i].DecoderCtx->sample_rate,
						av_get_sample_fmt_name(streamCtx[i].DecoderCtx->sample_fmt), streamCtx[i].DecoderCtx->channel_layout);
				}
				args = cargs;
			}

			ret = InitFilter(&FilterCtx[i], streamCtx[i].DecoderCtx,
				streamCtx[i].EncoderCtx, args.c_str(), filterSpec.c_str());
		}
		return ret;
	}
	
	int InitFilter(FilteringContext* filteringCtx, AVCodecContext *decoderCtx,
		AVCodecContext *encoderCtx, const char* args, const char *filterSpec) {

		int ret = 0;
		const AVFilter *buffersrc = NULL;
		const AVFilter *buffersink = NULL;

		AVFilterContext *buffersrcCtx = NULL;
		AVFilterContext *buffersinkCtx = NULL;
		AVFilterInOut *outputs = avfilter_inout_alloc();
		AVFilterInOut *inputs = avfilter_inout_alloc();
		AVFilterGraph *filterGraph = avfilter_graph_alloc();

		if (!outputs || !inputs || !filterGraph) {
			ret = AVERROR(ENOMEM);
			goto end;
		}

		if (decoderCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
			buffersrc = avfilter_get_by_name("buffer");
			buffersink = avfilter_get_by_name("buffersink");
			if (!buffersrc || !buffersink) {
				printf("filtering source or sink element not found\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}

			ret = avfilter_graph_create_filter(&buffersrcCtx, buffersrc, "in",
				args, NULL, filterGraph);
			if (ret < 0) {
				printf("Cannot create buffer source\n");
				goto end;
			}
			ret = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out",
				NULL, NULL, filterGraph);
			if (ret < 0) {
				printf("Cannot create buffer sink\n");
				goto end;
			}
			ret = av_opt_set_bin(buffersinkCtx, "pix_fmts",
				(uint8_t*)&encoderCtx->pix_fmt, sizeof(encoderCtx->pix_fmt),
				AV_OPT_SEARCH_CHILDREN);
			if (ret < 0) {
				printf("Cannot set output pixel format\n");
				goto end;
			}
		}
		else {
			buffersrc = avfilter_get_by_name("abuffer");
			buffersink = avfilter_get_by_name("abuffersink");

			if (!buffersrc || !buffersink) {
				printf("filtering source or sink element not found\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}

			if (!decoderCtx->channel_layout) {
				decoderCtx->channel_layout = av_get_default_channel_layout(decoderCtx->channels);
			}

			ret = avfilter_graph_create_filter(&buffersrcCtx, buffersrc, "in",
				args, NULL, filterGraph);

			if (ret) {
				printf("Cannot create audio buffer source\n");
				goto end;
			}

			ret = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out",
				NULL, NULL, filterGraph);
			if (ret) {
				printf("Cannot create audio buffer sink\n");
				goto end;
			}

			ret = av_opt_set_bin(buffersinkCtx, "sample_fmts",
				(uint8_t*)&encoderCtx->sample_fmt, sizeof(encoderCtx->sample_fmt),
				AV_OPT_SEARCH_CHILDREN);

			if (ret) {
				printf("Cannot set output sample format\n");
				goto end;
			}

			ret = av_opt_set_bin(buffersinkCtx, "channel_layouts",
				(uint8_t*)&encoderCtx->channel_layout,
				sizeof(encoderCtx->channel_layout), AV_OPT_SEARCH_CHILDREN);
			if (ret < 0) {
				printf("Cannot set output channel layout\n");
				goto end;
			}
			ret = av_opt_set_bin(buffersinkCtx, "sample_rates",
				(uint8_t*)&encoderCtx->sample_rate, sizeof(encoderCtx->sample_rate),
				AV_OPT_SEARCH_CHILDREN);

			if (ret < 0) {
				printf("Cannot set output sample rate\n");
				goto end;
			}
		}

		outputs->name = av_strdup("in");
		outputs->filter_ctx = buffersrcCtx;
		outputs->next = NULL;
		outputs->pad_idx = 0;

		inputs->name = av_strdup("out");
		inputs->filter_ctx = buffersinkCtx;
		inputs->next = NULL;
		inputs->pad_idx = 0;

		if (!outputs->name || !inputs->name) {
			ret = AVERROR(ENOMEM);
			goto end;
		}

		if ((ret = avfilter_graph_parse_ptr(filterGraph, filterSpec,
			&inputs, &outputs, NULL)) < 0) 
			goto end;

		if ((ret = avfilter_graph_config(filterGraph, NULL)) < 0)
			goto end;

		filteringCtx->BuffersrcCtx = buffersrcCtx;
		filteringCtx->BuffersinkCtx = buffersinkCtx;
		filteringCtx->FilterGraph = filterGraph;

	end:
		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);
		return ret;
	}

	void EndFilter(AVFormatContext *inputFmtCtx) {
		int i = 0;
		for (; i < inputFmtCtx->nb_streams; i++) {
			if (FilterCtx && FilterCtx[i].FilterGraph) {
				avfilter_graph_free(&FilterCtx[i].FilterGraph);
			}	
		}

		if (NULL != FilterCtx) {
			av_free(FilterCtx);
		}
	}


	int Buffersrc_add_frame(AVFrame* frame, unsigned int streamIndex) {
		return av_buffersrc_add_frame_flags(FilterCtx[streamIndex].BuffersrcCtx, frame, 0);
	}

	int Buffersink_get_frame(AVFrame* filterFrame, unsigned int streamIndex) {
		return av_buffersink_get_frame(FilterCtx[streamIndex].BuffersinkCtx, filterFrame);
	}
};