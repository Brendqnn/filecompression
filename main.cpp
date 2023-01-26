#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>


extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavcodec/codec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h>
    #include <libavutil/time.h>
}


int main(int argc, char** argv) {
    
    AVFormatContext *input_context = nullptr;
    AVFormatContext *output_context = nullptr;
    AVStream *stream = nullptr;
    const AVCodec *codec = avcodec_find_encoder_by_name("vp9");
    AVCodecContext *codec_context = nullptr;
    int fps = 60;
    
    if (avformat_open_input(&input_context, "res/replay.mp4", nullptr, nullptr) != 0){
        std::cout << "Error opening input file" << std::endl;
        return 1;
    }
    
    if (avformat_find_stream_info(input_context, nullptr) < 0) {
        std::cout << "Error finding stream info" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    int video_stream_index = av_find_best_stream(input_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    if (!codec) {
        std::cout << "codec not found" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    codec_context = avcodec_alloc_context3(NULL);
    
    codec_context->codec_id = AV_CODEC_ID_VP9;
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context->width = input_context->streams[video_stream_index]->codecpar->width;
    codec_context->height = input_context->streams[video_stream_index]->codecpar->height;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->bit_rate = 1000000;
    codec_context->gop_size = 12;
    codec_context->max_b_frames = 0;
    codec_context->time_base = (AVRational){1, fps};

    avcodec_parameters_to_context(codec_context, input_context->streams[video_stream_index]->codecpar);

    std::cout << "Codec name: " << avcodec_get_name(codec_context->codec_id) << std::endl;

    // open codec
    if (avcodec_open2(codec_context, codec, NULL) < 0) {
        std::cout << "Erorr Opening codec" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }
    
    if (avformat_alloc_output_context2(&output_context, nullptr, nullptr, "res/compressed-replay.mp4") < 0) {
        std::cout << "Error creating output context" << std::endl;
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    stream = avformat_new_stream(output_context, codec);

    if (!stream) {
        std::cout << "Error creating stream" << std::endl;
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }
    // copy codec to output file
    if (avcodec_parameters_from_context(stream->codecpar, codec_context)< 0) {
        std::cout << "Error copying codec context" << std::endl;
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }
    
    if (avio_open(&output_context->pb, "res/compressed-replay.mp4", AVIO_FLAG_WRITE) < 0) {
        std::cout << "Error opening output file" << std::endl;
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }


    if (avformat_write_header(output_context, nullptr) < 0) {
        std::cout << "Error writing header" << std::endl;
        avio_closep(&output_context->pb);
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    // set up AVFrame and AVPacket
    AVFrame *frame = av_frame_alloc();
    AVPacket packet;

    frame->width = codec_context->width;
    frame->height = codec_context->height;
    frame->format = codec_context->pix_fmt;
    int ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        std::cout << "Error allocating frame buffer" << std::endl;
        return 1;
    }

    // read and compress video frames
    while (av_read_frame(input_context, &packet) >= 0) {
        packet.stream_index = stream->index;
        packet.pts = av_rescale_q(packet.pts, input_context->streams[packet.stream_index]->time_base, stream->time_base);
        packet.dts = av_rescale_q(packet.dts, input_context->streams[packet.stream_index]->time_base, stream->time_base);
        packet.duration = av_rescale_q(packet.duration, input_context->streams[packet.stream_index]->time_base, stream->time_base);
        packet.pos = -1;
        av_packet_unref(&packet);
            
        // compress the video frames
        ret = avcodec_send_frame(codec_context, frame);
        if (ret < 0) {
            std::cout << "Error sending frame to codec: " << av_err2str(ret) << std::endl;
            break;
        }

        ret = avcodec_receive_packet(codec_context, &packet);
        if (ret < 0) {
            std::cout << "Error receiving packet from codec: " << av_err2str(ret) << std::endl;
            break;
        }

        if (av_interleaved_write_frame(output_context, &packet) < 0) {
            std::cout << "Error writing packet" << std::endl;
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(output_context);
    avio_closep(&output_context->pb);
    avcodec_close(codec_context);
    avformat_free_context(output_context);
    avformat_close_input(&input_context);
        
    
    return 0;
}