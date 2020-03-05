#include "ffmpeg_push_unittest.h"

int main(int argc, char *argv[])
{
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    FfmpegOutputer *pusher = NULL;
    int ret;
    int frame_index = 0;

    in_filename = "/Users/hsyuan/testfiles/yanxi-1080px25.264";

    //out_filename = "rtmp://172.16.1.25/live/test";
    //out_filename = "rtp://233.233.233.233:6666";
    //out_filename = "tcp://127.0.0.1:9000";
    //out_filename = "udp://127.0.0.1:9000";

    //Network
    avformat_network_init();

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    if (NULL == pusher) {
        pusher = new FfmpegOutputer();
        ret = pusher->OpenOutputStream(out_filename, ifmt_ctx);
        if (ret != 0){
            goto end;
        }
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    while (true) {
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) break;

        if (pkt.pts == AV_NOPTS_VALUE) {
            pkt.dts = pkt.pts = (1.0/30)*90*frame_index;
        }

        pusher->InputPacket(&pkt);
        av_packet_unref(&pkt);
        frame_index++;
        av_usleep(40000);
    }

    end:
    avformat_close_input(&ifmt_ctx);
    avformat_free_context(ifmt_ctx);
    delete pusher;
    return 0;
}
