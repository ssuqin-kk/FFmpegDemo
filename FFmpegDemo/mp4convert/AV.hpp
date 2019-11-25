#pragma once

#include <stdlib.h>
#include "FFMpegUtil.h"

#define AUDIO_G711_U   0x7110
#define AUDIO_G711_A   0x7111
#define AUDIO_G722     0x7221

#define MAX_PACKET_LEN (512*1024)

class AV {
protected:
	long long VideoStreamIndex = -1;
	long long AudioStreamIndex = -1;

	typedef enum {
		NALU_TYPE_SLICE = 1,
		NALU_TYPE_DPA = 2,
		NALU_TYPE_DPB = 3,
		NALU_TYPE_DPC = 4,
		NALU_TYPE_IDR = 5,
		NALU_TYPE_SEI = 6,
		NALU_TYPE_SPS = 7,
		NALU_TYPE_PPS = 8,
		NALU_TYPE_AUD = 9,
		NALU_TYPE_EOSEQ = 10,
		NALU_TYPE_EOSTREAM = 11,
		NALU_TYPE_FILL = 12,
	} NaluType;

	typedef enum {
		NALU_PRIORITY_DISPOSABLE = 0,
		NALU_PRIRITY_LOW = 1,
		NALU_PRIORITY_HIGH = 2,
		NALU_PRIORITY_HIGHEST = 3
	} NaluPriority;

	/**
	 * https://www.jianshu.com/p/82cc851df834
	 */
	typedef struct {
		int startcodeprefix_len;
		unsigned len;
		unsigned max_size;
		int forbidden_bit;
		int nal_reference_idc;
		int nal_unit_type;
		char *buf;
	} NALU_t;

	~AV() {
	}

	bool CheckPacket(AVPacket &packet) {
		int pos;
		NALU_t *nalu = (NALU_t*)calloc(1, sizeof(NALU_t));
		if (nalu == NULL){
			printf("Alloc NALU Error!\n");
			return false;
		}

		nalu->max_size = packet.size;
		nalu->buf = (char*)calloc(packet.size, sizeof(char));
		if (nalu->buf == NULL){
			free(nalu);
			printf("AllocNALU: n->buf!\n");
			return false;
		}

		bool checkPass = true;
		int offset = 0;
		
		while (offset < packet.size) {
			int data_length = GetAnnexbNALU(packet, nalu, offset);

			if (data_length <= 4) {
				printf("data_length[%d]!\n", data_length);
				checkPass = false;
			}

			if (nalu->forbidden_bit) {
				printf("forbidden_bit[%d]!\n", nalu->forbidden_bit);
				checkPass = false;
			}

			if (!checkPass) {
				break;
			}
			offset += data_length;
		}

		if (nalu){
			if (nalu->buf){
				free(nalu->buf);
				nalu->buf = NULL;
			}
			free(nalu);
		}
		return checkPass;
	}

	int FindStartCode2(unsigned char* buf, int offset){
		if (buf[offset] != 0 || buf[offset + 1] != 0 || buf[offset + 2] != 1) { // 0x000001?
			return 0;
		}
		return 1;
	}

	int FindStartCode3(unsigned char* buf, int offset){
		if (buf[offset] != 0 || buf[offset+1] != 0 
			|| buf[offset+2] != 0 || buf[offset+3] != 1) { // 0x00000001?
			return 0;
		}
		return 1;
	}

	/**
	 * https://blog.csdn.net/leixiaohua1020/article/details/50534369
	 * 这个函数输入为一个NAL结构体，主要功能为得到一个完整的NALU并保存在NALU_t的buf中，获取他的长度，填充F,IDC,TYPE位。
	 * 并且返回两个开始字符之间间隔的字节数，即包含有前缀的NALU的长度
	 */
	int GetAnnexbNALU(AVPacket &packet, NALU_t *nalu, int offset) {
		uint8_t *data = packet.data;
		int size = packet.size;

		if (!data && size < offset + 4) {
			return 0;
		}

		int pos = 0;
		int StartCodeFound;
		int rewind;

		nalu->startcodeprefix_len = 3;

		int info3;
		int info2 = FindStartCode2(data, offset);
		if (info2 == 1) {
			nalu->startcodeprefix_len = 3;
			pos = 3;
		} else {
			info3 = FindStartCode3(data, offset);
			if (info3 != 1) {
				return -1;
			}
			pos = 4;
			nalu->startcodeprefix_len = 4;
		}

		StartCodeFound = 0;
		info2 = 0;
		info3 = 0;

		while (!StartCodeFound) {
			if (pos + offset >= packet.size) {
				nalu->len = pos - nalu->startcodeprefix_len;
				memcpy(nalu->buf, &packet.data[offset + nalu->startcodeprefix_len], nalu->len);
				nalu->forbidden_bit = (nalu->buf[0] & 0x80) >> 7; //1 bit
				nalu->nal_reference_idc = (nalu->buf[0] & 0x60) >> 5; // 2 bit
				nalu->nal_unit_type = nalu->buf[0] & 0x1f;// 5 bit
				return pos;
			}
			pos++;
			info3 = FindStartCode3(&data[pos-4], offset);
			if (info3 != 1) {
				info2 = FindStartCode2(&data[pos - 3], offset);
			}
			StartCodeFound = (info2 == 1 || info3 == 1);
		}

		// Here the Start code, the complete NALU, and the next start code is in the Buf.  
		// The size of Buf is pos, pos+rewind are the number of bytes excluding the next
		// start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code
		rewind = (info3 == 1) ? -4 : -3;
		nalu->len = (pos + rewind) - nalu->startcodeprefix_len;
		memcpy(nalu->buf, &data[offset + nalu->startcodeprefix_len], nalu->len);
		nalu->forbidden_bit = (nalu->buf[0] & 0x80) >> 7; // 1 bit
		nalu->nal_reference_idc = (nalu->buf[0] & 0x60) >> 5; // 2 bit
		nalu->nal_unit_type = nalu->buf[0] & 0x1f; // 5 bit
		return pos + rewind;
	}

	void InitPacket(AVPacket *packet) {
		av_init_packet(packet);
		packet->data = NULL;
		packet->size = 0;
	}

	int InitInputFrame(AVFrame **frame) {
		if (!(*frame = av_frame_alloc())) {
			fprintf(stderr, "Could not allocate input frame\n");
			return AVERROR(ENOMEM);
		}
		return 0;
	}

	int InitOutputFrame(AVFrame **frame, AVCodecContext *outputCodecCtx, int frameSize) {
		int error;
		if (!(*frame = av_frame_alloc())) {
			fprintf(stderr, "Could not allocate output frame\n");
			return AVERROR_EXIT;
		}

		(*frame)->nb_samples = frameSize;
		(*frame)->channel_layout = outputCodecCtx->channel_layout;
		(*frame)->format = outputCodecCtx->sample_fmt;
		(*frame)->sample_rate = outputCodecCtx->sample_rate;

		if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
			fprintf(stderr, "Could not allocate output frame samples (error '%d')\n", error);
			av_frame_free(frame);
			return error;
		}
		return 0;
	}

	void PrintFrame(const AVFrame *frame) {
		const int n = frame->nb_samples * av_get_channel_layout_nb_channels(frame->channel_layout);
		const uint16_t *p = (uint16_t*)frame->data[0];
		const uint16_t *p_end = p + n;
		while (p < p_end) {
			fputc(*p & 0xff, stdout);
			fputc(*p >> 8 & 0xff, stdout);
			p++;
		}
		fflush(stdout);
	}

	unsigned short GetAudioEncodeType(const char* fileName) {
		unsigned short encodetype = AUDIO_G711_U;
		FILE *fp = fopen(fileName, "rb");
		if (fp) {
			char szheaderbuf[40] = { 0 };
			int ReadFileBufSize = fread(szheaderbuf, sizeof(unsigned char), sizeof(szheaderbuf), fp);
			if (ReadFileBufSize == sizeof(szheaderbuf)) {
				memcpy(&encodetype, szheaderbuf + 12, 2);  //g722:0x7221 g711u:0x7110
				if (0 == encodetype) encodetype = AUDIO_G711_U;
			}
			fclose(fp);
		}

		return encodetype;
	}
};
