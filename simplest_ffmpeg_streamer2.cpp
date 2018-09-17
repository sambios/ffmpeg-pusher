/**
 * 最简单的基于FFmpeg的推流器（推送RTMP）
 * Simplest FFmpeg Streamer (Send RTMP)
 * 
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 * 
 * 本例子实现了推送本地视频至流媒体服务器（以RTMP为例）。
 * 是使用FFmpeg进行流媒体推送最简单的教程。
 *
 * This example stream local media files to streaming media 
 * server (Use RTMP as example). 
 * It's the simplest FFmpeg streamer.
 * 
 */

#include <stdio.h>
#include <iostream>


#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#ifdef __cplusplus
};
#endif
#endif


AVRational instream_time_base = { 1, 1000 };
AVRational outstream_time_base = { 1, 90000 };
int frame_index = 0;

class FfmpegPusher {
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *codec = NULL;
    AVStream *out_stream[2];

    bool m_bOpened = false;
    std::string m_url;

public:
    FfmpegPusher() {

    }
    virtual ~FfmpegPusher()
    {

    }

    int Init(const std::string& url, AVFormatContext *ifmt_ctx)
    {
        int ret = 0;
        m_url = url;
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
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);

            out_stream[0] = avformat_new_stream(ofmt_ctx, codec);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }


            out_stream[0]->codec->pix_fmt = AV_PIX_FMT_YUV420P;
            out_stream[0]->codec->flags = CODEC_FLAG_GLOBAL_HEADER;
            out_stream[0]->codec->width = 512;
            out_stream[0]->codec->height = 288;
            AVRational timebase = { 1, 30 };
            out_stream[0]->codec->time_base = timebase;
            //out_stream[0]->codec->gop_size = 12;
            //out_stream[0]->codec->bit_rate = 512 << 10;
            //out_stream[0]->codec->codec_id = AV_CODEC_ID_H264;
            int size = ifmt_ctx->streams[0]->codec->extradata_size;
            out_stream[0]->codec->extradata = (uint8_t*)av_malloc(size);
            memcpy(out_stream[0]->codec->extradata, ifmt_ctx->streams[0]->codec->extradata, size);
            out_stream[0]->codec->extradata_size = size;

            out_stream[0]->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                out_stream[0]->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
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
            AVRational time_base = instream_time_base;
            AVRational time_base_q = { 1,AV_TIME_BASE };
            int64_t pts_time = av_rescale_q(pkt->dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

        }

        /* copy packet */
        //Convert PTS/DTS
        pkt->pts = av_rescale_q_rnd(pkt->pts, instream_time_base, outstream_time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, instream_time_base, outstream_time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, instream_time_base, outstream_time_base);
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



    int Deinit() {
        return 0;
    }



};
int main_test1()
{
    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    //Output


    while (true)
    {
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) break;

        if (pkt.pts == AV_NOPTS_VALUE) {
            //Write PTS
            AVRational time_base1 = instream_time_base;
            AVRational framerate = { 1,12 };
            //Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(framerate);
            //Parameters
            pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
        }

        pusher.Push(&pkt);

        //std::this_thread::sleep_for(40 * std::chrono::milliseconds());
        //av_usleep(40000);
    }

    //   avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", out_filename); //RTSP
    ////avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename); //RTMP
    ////avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP

    //if (!ofmt_ctx) {
    //	printf( "Could not create output context\n");
    //	ret = AVERROR_UNKNOWN;
    //	goto end;
    //}
    //ofmt = ofmt_ctx->oformat;
    //for (i = 0; i < ifmt_ctx->nb_streams; i++) {
    //	//Create output AVStream according to input AVStream
    //	AVStream *in_stream = ifmt_ctx->streams[i];
    //	AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
    //	if (!out_stream) {
    //		printf( "Failed allocating output stream\n");
    //		ret = AVERROR_UNKNOWN;
    //		goto end;
    //	}
    //	//Copy the settings of AVCodecContext
    //	ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
    //	if (ret < 0) {
    //		printf( "Failed to copy context from input to output stream codec context\n");
    //		goto end;
    //	}
    //	out_stream->codec->codec_tag = 0;
    //	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    //		out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    //}
    ////Dump Format------------------
    //av_dump_format(ofmt_ctx, 0, out_filename, 1);

    ////Open output URL
    //if (!(ofmt->flags & AVFMT_NOFILE)) {
    //	ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
    //	if (ret < 0) {
    //		printf( "Could not open output URL '%s'", out_filename);
    //		goto end;
    //	}
    //}

    ////Write file header
    //ret = avformat_write_header(ofmt_ctx, NULL);
    //if (ret < 0) {
    //	printf( "Error occurred when opening output URL\n");
    //	goto end;
    //}

    //start_time=av_gettime();
    //while (1) {
    //	AVStream *in_stream, *out_stream;
    //	//Get an AVPacket
    //	ret = av_read_frame(ifmt_ctx, &pkt);
    //	if (ret < 0)
    //		break;
    //	//FIX：No PTS (Example: Raw H.264)
    //	//Simple Write PTS
    //	if(pkt.pts==AV_NOPTS_VALUE){
    //		//Write PTS
    //		AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
    //		//Duration between 2 frames (us)
    //		int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
    //		//Parameters
    //		pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
    //		pkt.dts=pkt.pts;
    //		pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
    //	}
    //	//Important:Delay
    //	if(pkt.stream_index==videoindex){
    //		AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
    //		AVRational time_base_q={1,AV_TIME_BASE};
    //		int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
    //		int64_t now_time = av_gettime() - start_time;
    //		if (pts_time > now_time)
    //			av_usleep(pts_time - now_time);

    //	}

    //	in_stream  = ifmt_ctx->streams[pkt.stream_index];
    //	out_stream = ofmt_ctx->streams[pkt.stream_index];
    //	/* copy packet */
    //	//Convert PTS/DTS
    //	pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    //	pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    //	pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
    //	pkt.pos = -1;
    //	//Print to Screen
    //	if(pkt.stream_index==videoindex){
    //		printf("Send %8d video frames to output URL\n",frame_index);
    //		frame_index++;
    //	}
    //	//ret = av_write_frame(ofmt_ctx, &pkt);
    //	ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

    //	if (ret < 0) {
    //		printf( "Error muxing packet\n");
    //		break;
    //	}
    //	
    //	av_free_packet(&pkt);
    //	
    //}
    //Write file trailer
    //av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename, *out_filename;
	int ret, i;
	int videoindex=-1;
	
	int64_t start_time=0;
	//in_filename  = "cuc_ieschool.mov";
	//in_filename  = "cuc_ieschool.mkv";
	//in_filename  = "cuc_ieschool.ts";
	//in_filename  = "cuc_ieschool.mp4";
	//in_filename  = "cuc_ieschool.h264";
	in_filename  = "cuc_ieschool.flv";//输入URL（Input file URL）
	//in_filename  = "shanghai03_p.h264";
	
	//out_filename = "rtmp://172.16.1.25/live/test";//输出 URL（Output URL）[RTMP]
	//out_filename = "rtp://233.233.233.233:6666";//输出 URL（Output URL）[UDP]
    out_filename = "rtsp://172.16.1.25:8554/test";//输出 URL（Output URL）[RTSP]

	av_register_all();
	//Network
	avformat_network_init();

    FfmpegPusher pusher;
   
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		printf( "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto end;
	}

	for(i=0; i<ifmt_ctx->nb_streams; i++) 
		if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}

    pusher.Init(out_filename, ifmt_ctx);

	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	//Output
    
   
    while (true)
    {
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) break;

        if (pkt.pts == AV_NOPTS_VALUE) {
            //Write PTS
            AVRational time_base1 = instream_time_base;
            AVRational framerate = { 1,12 };
            //Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(framerate);
            //Parameters
            pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
        }

        pusher.Push(&pkt);
        
        //std::this_thread::sleep_for(40 * std::chrono::milliseconds());
        //av_usleep(40000);
    }
	
 //   avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", out_filename); //RTSP
	////avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename); //RTMP
	////avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP

	//if (!ofmt_ctx) {
	//	printf( "Could not create output context\n");
	//	ret = AVERROR_UNKNOWN;
	//	goto end;
	//}
	//ofmt = ofmt_ctx->oformat;
	//for (i = 0; i < ifmt_ctx->nb_streams; i++) {
	//	//Create output AVStream according to input AVStream
	//	AVStream *in_stream = ifmt_ctx->streams[i];
	//	AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
	//	if (!out_stream) {
	//		printf( "Failed allocating output stream\n");
	//		ret = AVERROR_UNKNOWN;
	//		goto end;
	//	}
	//	//Copy the settings of AVCodecContext
	//	ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
	//	if (ret < 0) {
	//		printf( "Failed to copy context from input to output stream codec context\n");
	//		goto end;
	//	}
	//	out_stream->codec->codec_tag = 0;
	//	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	//		out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	//}
	////Dump Format------------------
	//av_dump_format(ofmt_ctx, 0, out_filename, 1);

	////Open output URL
	//if (!(ofmt->flags & AVFMT_NOFILE)) {
	//	ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
	//	if (ret < 0) {
	//		printf( "Could not open output URL '%s'", out_filename);
	//		goto end;
	//	}
	//}

	////Write file header
	//ret = avformat_write_header(ofmt_ctx, NULL);
	//if (ret < 0) {
	//	printf( "Error occurred when opening output URL\n");
	//	goto end;
	//}

	//start_time=av_gettime();
	//while (1) {
	//	AVStream *in_stream, *out_stream;
	//	//Get an AVPacket
	//	ret = av_read_frame(ifmt_ctx, &pkt);
	//	if (ret < 0)
	//		break;
	//	//FIX：No PTS (Example: Raw H.264)
	//	//Simple Write PTS
	//	if(pkt.pts==AV_NOPTS_VALUE){
	//		//Write PTS
	//		AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
	//		//Duration between 2 frames (us)
	//		int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
	//		//Parameters
	//		pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
	//		pkt.dts=pkt.pts;
	//		pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
	//	}
	//	//Important:Delay
	//	if(pkt.stream_index==videoindex){
	//		AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
	//		AVRational time_base_q={1,AV_TIME_BASE};
	//		int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
	//		int64_t now_time = av_gettime() - start_time;
	//		if (pts_time > now_time)
	//			av_usleep(pts_time - now_time);

	//	}

	//	in_stream  = ifmt_ctx->streams[pkt.stream_index];
	//	out_stream = ofmt_ctx->streams[pkt.stream_index];
	//	/* copy packet */
	//	//Convert PTS/DTS
	//	pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	//	pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	//	pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
	//	pkt.pos = -1;
	//	//Print to Screen
	//	if(pkt.stream_index==videoindex){
	//		printf("Send %8d video frames to output URL\n",frame_index);
	//		frame_index++;
	//	}
	//	//ret = av_write_frame(ofmt_ctx, &pkt);
	//	ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

	//	if (ret < 0) {
	//		printf( "Error muxing packet\n");
	//		break;
	//	}
	//	
	//	av_free_packet(&pkt);
	//	
	//}
	//Write file trailer
	//av_write_trailer(ofmt_ctx);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		printf( "Error occurred.\n");
		return -1;
	}
	return 0;
}

extern int main_test2();

int main(int argc, char *argv[]) {
    main_test1();
    //main_test2();
}



