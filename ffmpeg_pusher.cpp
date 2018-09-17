#include "ffmpeg_pusher.h"


int main(int argc, char *argv[])
{

    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int videoindex = -1;

    int64_t start_time = 0;
    //in_filename  = "cuc_ieschool.mov";
    //in_filename  = "cuc_ieschool.mkv";
    //in_filename  = "cuc_ieschool.ts";
    //in_filename  = "cuc_ieschool.mp4";
    //in_filename  = "cuc_ieschool.h264";
    in_filename = "Y:\\testfiles\\cuc_ieschool.flv";//输入URL（Input file URL）
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
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    for (i = 0; i<ifmt_ctx->nb_streams; i++)
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }

    pusher.Init(out_filename, 0, ifmt_ctx);

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    //Output


    while (true)
    {
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) break;

        pusher.Push(&pkt);

        //std::this_thread::sleep_for(40 * std::chrono::milliseconds());
        //av_usleep(40000);
    }

   
    //av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}
