#pragma once

#include "FFMpegHeader.h"
#include "FFMpegUtil.h"

class AudioResample {
private:
	int GetFormatFromSampleFmt(const char **fmt, enum AVSampleFormat sample_fmt) {
		int i;
		struct sample_fmt_entry {
			enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
		} sample_fmt_entries[] = {
			{ AV_SAMPLE_FMT_U8, "u8", "u8" },
			{ AV_SAMPLE_FMT_S16, "s16be", "s16le" },
			{ AV_SAMPLE_FMT_S32, "s32be", "s32le" },
			{ AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
			{ AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
		};
		*fmt = NULL;
		for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
			struct sample_fmt_entry *entry = &sample_fmt_entries[i];
			if (sample_fmt == entry->sample_fmt) {
				*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
				return 0;
			}
		}
		fprintf(stderr,
			"Sample format %s not supported as output format\n",
			av_get_sample_fmt_name(sample_fmt));
		return AVERROR(EINVAL);
	}

	/**
	 * Fill dst buffer with nb_samples, generated starting from t.
	 */
	void FillSamples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t) {
		int i, j;
		double tincr = 1.0 / sample_rate, *dstp = dst;
		const double c = 2 * M_PI * 440.0;
		/* generate sin tone with 440Hz frequency and duplicated channels */
		for (i = 0; i < nb_samples; i++) {
			*dstp = sin(c * *t);
			for (j = 1; j < nb_channels; j++)
				dstp[j] = dstp[0];
			dstp += nb_channels;
			*t += tincr;
		}
	}
public:
	int InitFifo(int streamIndex, StreamContext *streamCtx, AVAudioFifo **fifo) {
		if (streamIndex < 0) {
			return 0;
		}

		if (!(*fifo = av_audio_fifo_alloc(streamCtx[streamIndex].EncoderCtx->sample_fmt,
			streamCtx[streamIndex].EncoderCtx->channels, 1))) {
			fprintf(stderr, "Could not allocate FIFO\n");
			return AVERROR(ENOMEM);
		}
		return 0;
	}

	int InitResampler(int streamIndex, StreamContext *streamCtx,
		SwrContext **resampleCtx) {
		int error;

		if (streamIndex < 0) {
			return 0;
		}

		*resampleCtx = swr_alloc_set_opts(NULL,
			av_get_default_channel_layout(streamCtx[streamIndex].EncoderCtx->channels),
			streamCtx[streamIndex].EncoderCtx->sample_fmt,
			streamCtx[streamIndex].EncoderCtx->sample_rate,
			av_get_default_channel_layout(streamCtx[streamIndex].DecoderCtx->channels),
			streamCtx[streamIndex].DecoderCtx->sample_fmt,
			streamCtx[streamIndex].DecoderCtx->sample_rate,
			0, NULL);

		if (!*resampleCtx) {
			fprintf(stderr, "Could not allocate resample context\n");
			return AVERROR(ENOMEM);
		}

		if ((error = swr_init(*resampleCtx)) < 0) {
			fprintf(stderr, "Could not open resample context\n");
			swr_free(resampleCtx);
			return error;
		}
		return 0;
	}

	int InitConvertedSamples(uint8_t ***convertedInputSamples, unsigned int streamIndex, StreamContext *streamCtx, int frameSize) {
		int error;
		if (!(*convertedInputSamples = (uint8_t **)calloc(streamCtx[streamIndex].EncoderCtx->channels,
			sizeof(**convertedInputSamples)))) {
			fprintf(stderr, "Could not allocate converted input sample pointers\n");
			return AVERROR(ENOMEM);
		}
		
		if ((error = av_samples_alloc(*convertedInputSamples, NULL,
			streamCtx[streamIndex].EncoderCtx->channels,
			frameSize,
			streamCtx[streamIndex].EncoderCtx->sample_fmt, 0)) < 0) {
			fprintf(stderr,
				"Could not allocate converted input samples (error '%d')\n",error);
			av_freep(&(*convertedInputSamples)[0]);
			free(*convertedInputSamples);
			return error;
		}
		return 0;
	}

	int ConvertSamples(const uint8_t **inputData, uint8_t **convertedData, const int frameSize, SwrContext *resampleCtx) {
		int error;
		if ((error = swr_convert(resampleCtx,
			convertedData, frameSize,
			inputData, frameSize)) < 0) {
			fprintf(stderr, "Could not convert input samples (error '%d')\n", error);
			return error;
		}
		return 0;
	}
	
	int AddSamplesToFifo(AVAudioFifo *fifo, uint8_t **convertedInputSamples, const int frame_size) {
		int error;
		if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
			fprintf(stderr, "Could not reallocate FIFO\n");
			return error;
		}
	
		if (av_audio_fifo_write(fifo, (void **)convertedInputSamples,
			frame_size) < frame_size) {
			fprintf(stderr, "Could not write data to FIFO\n");
			return AVERROR_EXIT;
		}
		return 0;
	}
};