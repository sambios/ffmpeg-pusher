#pragma once

#include <iostream>
#include <thread>
#include <chrono>
#include <assert.h>

#ifdef __cplusplus 
extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"

}
#endif 


enum FfmpegPushType {
    FPT_INPUT_TYPE_YUV420P=0,
    FPT_INPUT_TYPE_H264,
};


typedef struct ffmpeg_enc_param_struct
{
    int bitrate; // Bitrate in bytes
    int width;
    int height;
    int fps;
    int gop_size;
    int max_b_frames;
    int pix_fmt;
    int min_qp;
    int max_qp;
}ffmpeg_enc_param_t;


class FfmpegPusher {

    FfmpegPushType m_pushType;

    //Encoder Context
    AVCodecContext *m_h264Enc = NULL;
    //output Context
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *codec = NULL;
    AVStream *out_stream[2];

    AVRational m_instream_time_base = { 1, 1000 };

    bool m_bOpened = false;
    int frame_index = 0;
    std::string m_url;

    int CreateEncoder(ffmpeg_enc_param_t *pEncParam)
    {
        int ret = 0;
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        // Init Encoder if it needed
        m_h264Enc = avcodec_alloc_context3(codec);
        assert(m_h264Enc != NULL);

        /* put sample parameters */
        m_h264Enc->bit_rate = pEncParam->bitrate;
        /* resolution must be a multiple of two */
        m_h264Enc->width = pEncParam->width;
        m_h264Enc->height = pEncParam->height;

        /* frames per second */
        m_h264Enc->time_base.den = pEncParam->fps;
        m_h264Enc->time_base.num = 1;
        m_h264Enc->framerate.num = pEncParam->fps;
        m_h264Enc->framerate.den = 1;

        m_h264Enc->gop_size = pEncParam->gop_size; /* emit one intra frame every ten frames */
        m_h264Enc->max_b_frames = pEncParam->max_b_frames;
        m_h264Enc->pix_fmt = (enum AVPixelFormat) pEncParam->pix_fmt;
        m_h264Enc->qmax = pEncParam->max_qp;
        m_h264Enc->qmin = pEncParam->min_qp;
        m_h264Enc->delay = 0;

        AVDictionary *options;
        av_dict_set(&options, "preset", "medium", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "profile", "baseline", 0);

        ret = avcodec_open2(m_h264Enc, codec, &options);
        if (ret < 0) {

            printf("avcodec_open2 failed!");
            return -1;
        }

        av_dict_free(&options);
        return 0;
    }

public:
    FfmpegPusher(FfmpegPushType type = FPT_INPUT_TYPE_YUV420P):m_pushType(type) {
        
    }
    virtual ~FfmpegPusher()
    {

    }

    int Init(const std::string& url, ffmpeg_enc_param_t *pEncParam, AVFormatContext *ifmt_ctx)
    {
        int ret = 0;
        m_url = url;

        // Init output context
        avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", url.c_str()); //RTSP

        if (!ofmt_ctx) {
            printf("Could not create output context\n");
            return AVERROR_UNKNOWN;
        }

        ofmt = ofmt_ctx->oformat;
        if (ifmt_ctx != nullptr) {
            CopyInStreamContext(ifmt_ctx);
        }
        else
        {
            if (pEncParam) {
                ret = CreateEncoder(pEncParam);
                if (ret < 0) {
                    printf("Create H264 Encoder err=%d\n", ret);
                    return ret;
                }
            }
        }

        //out_stream->codec->video
        //
        av_dump_format(ofmt_ctx, 0, url.c_str(), 1);

        return 0;

    }

    int CopyInStreamContext(AVFormatContext *ifmt_ctx) {
        int ret = 0;
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            //Create output AVStream according to input AVStream
            AVStream *in_stream = ifmt_ctx->streams[i];
            out_stream[i] = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }

            //Copy the settings of AVCodecContext
            ret = avcodec_copy_context(out_stream[i]->codec, in_stream->codec);
            if (ret < 0) {
                printf("Failed to copy context from input to output stream codec context\n");
                return ret;
            }
            out_stream[i]->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream[i]->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        return 0;
    }


    int Push(AVPacket *pkt)
    {
        int ret = 0;
        static int64_t start_time = 0;
        if (start_time == 0) {
            start_time = av_gettime();
        }

        if (pkt->stream_index > 0) return -1;

        //Open output URL
        if (!m_bOpened) {
            if (!(ofmt->flags & AVFMT_NOFILE)) {
                ret = avio_open(&ofmt_ctx->pb, m_url.c_str(), AVIO_FLAG_WRITE);
                if (ret < 0) {
                    printf("Could not open output URL '%s'", m_url.c_str());
                    return -1;
                }
            }

            //Write file header
            ret = avformat_write_header(ofmt_ctx, NULL);
            if (ret < 0) {
                printf("Error occurred when opening output URL\n");
                return -2;
            }

            m_bOpened = true;
        }


        //Important:Delay
        if (pkt->stream_index == 0) {
            AVRational time_base = { 1, 1000 };
            AVRational time_base_q = { 1,AV_TIME_BASE };
            int64_t pts_time = av_rescale_q(pkt->dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

        }

        /* copy packet */
        //Convert PTS/DTS
        pkt->pts = av_rescale_q_rnd(pkt->pts, m_instream_time_base, out_stream[0]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, m_instream_time_base, out_stream[0]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, m_instream_time_base, out_stream[0]->time_base);
        pkt->pos = -1;

        //Print to Screen
        if (pkt->stream_index == 0) {
            printf("Send %8d video frames to output URL\n", frame_index);
            frame_index++;
        }

        printf("pkt.pts = %d, pkt.dts=%d, pkt.duration=%d\n", pkt->pts, pkt->dts, pkt->duration);
        ret = av_interleaved_write_frame(ofmt_ctx, pkt);

        if (ret < 0) {
            printf("Error muxing packet\n");
        }

        av_packet_unref(pkt);
        return 0;
    }

    int PushAVFrame(AVFrame *frame) {

        AVPacket pkt;
        av_init_packet(&pkt);
        int got_packet = 0;

        int ret = avcodec_encode_video2(m_h264Enc, &pkt, frame, &got_packet);
        if (!got_packet) {
            return 0;
        }

        if (out_stream[0] == nullptr) {
            out_stream[0] = avformat_new_stream(ofmt_ctx, codec);
            if (!out_stream[0]) {
                printf("Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }

            //Copy the settings of AVCodecContext
            ret = avcodec_copy_context(out_stream[0]->codec, m_h264Enc);
            if (ret < 0) {
                printf("Failed to copy context from input to output stream codec context\n");
                return ret;
            }

            out_stream[0]->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream[0]->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        Push(&pkt);
    }


    int Deinit() {
        return 0;
    }



};