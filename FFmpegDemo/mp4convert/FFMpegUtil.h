#pragma once
#include "FFMpegHeader.h"
#include <string>
#include<sstream>
using namespace std;

typedef struct StreamContext {
	AVCodecContext *DecoderCtx = NULL;
	AVCodecContext *EncoderCtx = NULL;
	bool Enabled = false;
} StreamContext;

class FFMpegUtil {
public:
    static const int LOG_SECTION = 105;

	/**
	 * 视频输入过滤器参数字符串
	 * return "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d"
	 */
	static std::string VideoInputFiltersArgs(int width, int height, int pix_fmt, AVRational timebase, AVRational sampleaspect) {
		std::stringstream ss;
		ss << "video_size=" << width << "x" << height << ":pix_fmt=" << pix_fmt << ":time_base=" << timebase.num << "/" << timebase.den << ":pixel_aspect=" << sampleaspect.num << "/" << sampleaspect.den;
		return ss.str();
	}

	static std::string VideoOutput_null() {
		return "null";
	}

	/**
	 * 缩放
	 * return [in]scale=%d:%d[out]
	 */
	static std::string VideoOutput_scale(int width, int height) {
		std::stringstream ss;
		ss << "[in]scale=" << width << ":" << height << "[out]";
		return ss.str();
	}

	/** 
	 * 水平翻转
	 */
	static std::string VideoOutput_hflip() {
		return "hflip";
	}

	/**
	 * 画方块
	 * x,y 方块左上角位置
	 * width,height 方块宽高
	 * color 方块边框颜色，如灰色0.5：pink@0.5
	 * return drawbox=x=100:y=100:w=100:h=100:color=pink@1
	 */
	static std::string VideoOutput_drawbox(int x, int y, int width, int height, const char* color) {
		std::stringstream ss;
		ss << "drawbox=x=" << x << ":y=" << y << ":w=" << width << ":h=" << height << ":color=" << color;
		return ss.str();
	}

	/**
	 * filters_descr = "lutyuv='u=128:v=128'";
	 * filters_descr = "scale=78:24,transpose=cclock";
	 * filters_descr = "boxblur";
	 * filters_descr = "movie=my_logo.png[wm];[in][wm]overlay=5:5[out]";
	 * filters_descr =  "crop=2/3*in_w:2/3*in_h";
	 * 音频输入过滤器参数字符串
	 * return "sample_fmt=%s:time_base=%d/%d:sample_rate=%d:channel_layout=0x%x"
	 */
	static std::string AudioInputFiltersArgs(AVSampleFormat sample_fmt, int sample_rate, AVRational timeBase, uint64_t channel_layout = -1) {
		std::stringstream ss;
		ss << "sample_fmt=" << av_get_sample_fmt_name(sample_fmt) << ":time_base=" << timeBase.num << ":" << timeBase.den << ":sample_rate=" << sample_rate << ":channel_layout=";
		if (channel_layout == -1) {
			ss << "mono";
		} else {
			ss << channel_layout;
		}
		return ss.str();
	}

	/**
	 * "aformat=sample_fmts=%s:sample_rates=%d:channel_layouts=0x%x"
	 */
	static std::string AudioOutputArgs(AVSampleFormat sample_fmt, int sample_rate, uint64_t channel_layout = -1) {
		std::stringstream ss;
		ss << "aformat=sample_fmts=" << av_get_sample_fmt_name(sample_fmt) << ":sample_rates=" << sample_rate << ":channel_layouts=";
		if (channel_layout == -1) {
			ss << "stereo";
		} else {
			ss << channel_layout;
		}
		return ss.str();
	}

    static AVFrame * CreateAudioFrame(AVCodecContext* context) {
        AVFrame *frame = av_frame_alloc();
		if (frame == NULL) {
			return NULL;
		}

        frame->format = context->sample_fmt;
        frame->channel_layout = context->channel_layout;
        frame->sample_rate = context->sample_rate;
        frame->nb_samples = context->frame_size;
        int err = av_frame_get_buffer(frame, 0);
        return frame;
    }

    static AVFrame * CreateAudioFrame(int sample_fmt, unsigned long long channel_layout, int sample_rate, int nb_samples) {
        AVFrame *frame = av_frame_alloc();
        frame->format = sample_fmt;
        frame->channel_layout = channel_layout;
        frame->sample_rate = sample_rate;
        frame->nb_samples = nb_samples;
        int err = av_frame_get_buffer(frame, 0);
        return frame;
    }

    static AVFrame* CreateVideoFrame(AVCodecContext* context) {
        AVFrame *frame = av_frame_alloc();
        frame->width = context->width;
        frame->height = context->height;
        frame->format = context->pix_fmt;
        av_frame_get_buffer(frame, 0);
        return frame;
    }

    static AVFrame* CreateVideoFrame(int width, int height, int pix_fmt) {
        AVFrame *frame = av_frame_alloc();
        frame->width = width;
        frame->height = height;
        frame->format = pix_fmt;
        av_frame_get_buffer(frame, 0);
        return frame;
    }

    static void InitPacket(AVPacket& packet) {
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;
    }


	static int snprintf(char *s, size_t n, const char *fmt, ...) {
		va_list ap;
		int ret;

		va_start(ap, fmt);
		ret = vsnprintf(s, n, fmt, ap);
		va_end(ap);
		return ret;
	}

	static void Register() {
		av_register_all();
		avfilter_register_all();
	}
};