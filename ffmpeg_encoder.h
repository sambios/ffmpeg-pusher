//
// Created by hsyuan on 2020-03-05.
//

#ifndef FFMPEG_PUSHER_FFMPEG_ENCODER_H
#define FFMPEG_PUSHER_FFMPEG_ENCODER_H

struct FfmpegEncParam
{
    AVCodecID codec_id;
    int bitrate;
    int width;
    int height;
    int fps;
    int gop_size;
    int max_b_frames;
    int pix_fmt;
    int min_qp;
    int max_qp;
};

class FfmpegEncoder
{
    AVCodecContext *m_enc_ctx;

public:
    FfmpegEncoder();
    FfmpegEncoder(FfmpegEncParam *enc_param){
        int ret = Init(enc_param);
        assert(ret == 0);
    }

    virtual ~FfmpegEncoder(){

    }

    int Init(FfmpegEncParam *param){
        int ret = 0;
        AVCodec *codec = avcodec_find_encoder(param->codec_id);
        if (nullptr == codec) {
            std::cout << "ffmpeg not support codec_id=" <<param->codec_id << std::endl;
            return -1;
        }

        m_enc_ctx = avcodec_alloc_context3(codec);
        assert(m_enc_ctx != nullptr);

        m_enc_ctx->bit_rate = pEncParam->bitrate;
        m_enc_ctx->width = pEncParam->width;
        m_enc_ctx->height = pEncParam->height;

        m_enc_ctx->time_base.den = pEncParam->fps;
        m_enc_ctx->time_base.num = 1;
        m_enc_ctx->framerate.num = pEncParam->fps;
        m_enc_ctx->framerate.den = 1;

        m_enc_ctx->gop_size = pEncParam->gop_size;
        m_enc_ctx->max_b_frames = pEncParam->max_b_frames;
        m_enc_ctx->pix_fmt = (enum AVPixelFormat) pEncParam->pix_fmt;
        m_enc_ctx->qmax = pEncParam->max_qp;
        m_enc_ctx->qmin = pEncParam->min_qp;
        m_enc_ctx->delay = 0;

        AVDictionary *options;
        av_dict_set(&options, "preset", "medium", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "profile", "baseline", 0);

        ret = avcodec_open2(m_h264Enc, codec, &options);
        if (ret < 0) {
            std::cout << "avcodec_open2 failed!" << std::endl;
            return -1;
        }

        return 0;
    }
    int EncodeFrame(AVFrame *frame, std::function<void(AVPacket* pkt)> cb)
    {
        int got_packet = 0;
        AVPacket pkt;
        av_init_packet(&pkt);
        int ret = avcodec_encode_video2(m_enc_ctx, &pkt, frame, &got_packet);
        if (!got_packet) {
            return 0;
        }

        if (cb != nullptr) cb(pkt);
        av_packet_unref(&pkt);
        return 0;
    }

    int Flush(std::function<void(AVPacket* pkt)> cb)
    {
        return 0;
    }
};



#endif //FFMPEG_PUSHER_FFMPEG_ENCODER_H
