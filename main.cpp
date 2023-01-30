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
    #include <libavutil/frame.h>
}


int main(int argc, char** argv) {
    
    AVFormatContext *input_context = nullptr;
    AVFormatContext *output_context = nullptr;
    const AVCodec *decoder = nullptr;
    AVCodecContext *decoder_context = nullptr;
    AVStream *stream = nullptr;
    int fps = 60;
    int64_t pts = 0;

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
    decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoder_context = avcodec_alloc_context3(decoder);
    
    if (avcodec_parameters_to_context(decoder_context, input_context->streams[video_stream_index]->codecpar) < 0) {
        std::cerr << "Failed to copy codec parameters." << std::endl;
        return -1;
    }

    if (avcodec_open2(decoder_context, decoder, nullptr) < 0) {
        std::cerr << "Error: could not open codec" << std::endl;
        avcodec_free_context(&decoder_context);
        avformat_close_input(&input_context);
        return 1;
    }

    const AVCodec *output_codec = avcodec_find_encoder(AV_CODEC_ID_H265);
    AVCodecContext *output_codec_context = avcodec_alloc_context3(output_codec);

    output_codec_context->bit_rate = 8000000;
    output_codec_context->width = decoder_context->width;
    output_codec_context->height = decoder_context->height;
    output_codec_context->pix_fmt = output_codec->pix_fmts[0];
    output_codec_context->time_base = (AVRational){1, fps};

    if (avcodec_open2(output_codec_context, output_codec, nullptr) < 0) {
        std::cout << "Error: could not open codec for output file" << std::endl;
        avcodec_free_context(&decoder_context);
        avcodec_free_context(&output_codec_context);
        avformat_close_input(&input_context);
        return 1;
    }
    // Allocate output context
    avformat_alloc_output_context2(&output_context, nullptr, nullptr, "res/compressed-copy.mp4");

    // Add a new stream to the output file
    stream = avformat_new_stream(output_context, decoder);

    // Copy codec parameters from input to output stream
    avcodec_parameters_from_context(stream->codecpar, output_codec_context);

    // Open output file
    if (!(output_context->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_context->pb, "res/compressed-copy.mp4", AVIO_FLAG_WRITE) < 0) {
            std::cout << "Error: could not open output file" << std::endl;
            avcodec_free_context(&decoder_context);
            avcodec_free_context(&output_codec_context);
            avformat_close_input(&input_context);
            return 1;
        }
    }

    // Write the header
    if (avformat_write_header(output_context, nullptr) < 0) {
        std::cout << "Error: could not write header to output file" << std::endl;
        avcodec_free_context(&decoder_context);
        avcodec_free_context(&output_codec_context);
        avio_closep(&output_context->pb);
        avformat_close_input(&input_context);
    }
    
    AVPacket packet;
    packet.data = NULL;
    packet.size = 0;

    AVFrame *frame = av_frame_alloc();
    
    frame->width = output_codec_context->width;
    frame->height = output_codec_context->height;
    frame->format = output_codec_context->pix_fmt;
    frame->time_base = output_codec_context->time_base;
    

    av_frame_get_buffer(frame, 0);

    while (av_read_frame(input_context, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            if (avcodec_send_packet(decoder_context, &packet) < 0) {
                std::cout << "Error sending packet" << std::endl;
                break;
            }
            while (int ret = avcodec_receive_frame(decoder_context, frame) >= 0) {
                frame->pts = pts;
                pts += av_rescale_q(1, output_codec_context->time_base, stream->time_base);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    std::cout << "Error returning frame" << std::endl;
                    break;
                }
                if (avcodec_send_frame(output_codec_context, frame) < 0 ) {
                    std::cerr << "Error sending frame" << std::endl;
                    break;
                }
                while (avcodec_receive_packet(output_codec_context, &packet) >= 0) {
                    if (av_interleaved_write_frame(output_context, &packet) < 0) {
                        std::cerr << "Error writing packet" << std::endl;
                        break;
                    }
                }
            }
        }
        av_packet_unref(&packet);   
    }


    // write the trailer and close the output file
    av_write_trailer(output_context);
    avio_closep(&output_context->pb);

    // close the decoder and encoder
    avcodec_free_context(&output_codec_context);
    avcodec_free_context(&decoder_context);

    // close the input file
    avformat_close_input(&input_context);

    return 0;
}