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
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_context = nullptr;
    AVStream *stream = nullptr;
    int fps = 60;

    // Open input file
    if (avformat_open_input(&input_context, "res/copy.mp4", nullptr, nullptr) != 0){
        std::cout << "Error opening input file" << std::endl;
        return 1;
    }

    // Find stream information
    if (avformat_find_stream_info(input_context, nullptr) < 0) {
        std::cout << "Error finding stream info" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    // Find the video stream
    int video_stream_index = -1;
    for (unsigned int i = 0; i < input_context->nb_streams; i++) {
        if (input_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        std::cout << "Error: no video stream found in input file" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    // Open the codec
    codec = avcodec_find_decoder(input_context->streams[video_stream_index]->codecpar->codec_id);
    codec_context = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_context, input_context->streams[video_stream_index]->codecpar);
    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        std::cout << "Error: could not open codec" << std::endl;
        avcodec_free_context(&codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    // Allocate output context
    avformat_alloc_output_context2(&output_context, nullptr, nullptr, "res/compressed-copy.mp4");

    // Add a new stream to the output file
    stream = avformat_new_stream(output_context, codec);

    // Copy codec parameters from input to output stream
    avcodec_parameters_from_context(stream->codecpar, codec_context);
    // stream->codecpar->codec_tag = 0;
    // stream->codecpar->codec_id = AV_CODEC_ID_VP9;
    const AVCodec *out_codec = avcodec_find_encoder(AV_CODEC_ID_H265);
    AVCodecContext *out_codec_context = avcodec_alloc_context3(out_codec);

    // set codec context parameters
    out_codec_context->bit_rate = 1000000;
    out_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    out_codec_context->width = codec_context->width;
    out_codec_context->height = codec_context->height;
    out_codec_context->gop_size = 12;
    out_codec_context->max_b_frames = codec_context->max_b_frames;
    out_codec_context->pix_fmt = codec_context->pix_fmt;
    out_codec_context->time_base = (AVRational){1, fps};

    if (avcodec_open2(out_codec_context, out_codec, nullptr) < 0) {
        std::cout << "Error: could not open codec for output file" << std::endl;
        avcodec_free_context(&codec_context);
        avcodec_free_context(&out_codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    // Open output file
    if (!(output_context->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_context->pb, "res/compressed-copy.mp4", AVIO_FLAG_WRITE) < 0) {
            std::cout << "Error: could not open output file" << std::endl;
            avcodec_free_context(&codec_context);
            avcodec_free_context(&out_codec_context);
            avformat_close_input(&input_context);
            return 1;
        }
    }

    // Write the header
    if (avformat_write_header(output_context, nullptr) < 0) {
        std::cout << "Error: could not write header to output file" << std::endl;
        avcodec_free_context(&codec_context);
        avcodec_free_context(&out_codec_context);
        avio_closep(&output_context->pb);
        avformat_close_input(&input_context);
    }
    
    AVPacket packet;

    AVFrame *frame = av_frame_alloc();

    frame->time_base = out_codec_context->time_base;
    frame->width = out_codec_context->width;
    frame->height = out_codec_context->height;
    frame->format = out_codec_context->pix_fmt;

    while (av_read_frame(input_context, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_context, &packet) < 0) {
                std::cout << "Error sending packet" << std::endl;
                break;
            }
            if (avcodec_receive_frame(codec_context, frame) < 0) {
                std::cout << "Error returning frame" << std::endl;
                break;
            }

        }   
    }


    // write the trailer and close the output file
    av_write_trailer(output_context);
    avio_closep(&output_context->pb);

    // close the decoder and encoder
    avcodec_free_context(&out_codec_context);
    avcodec_free_context(&codec_context);

    // close the input file
    avformat_close_input(&input_context);

    return 0;
}