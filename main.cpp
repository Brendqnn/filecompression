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
    #include <libavutil/audio_fifo.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/opt.h>
    #include <libswresample/swresample.h>
    #include <libavutil/channel_layout.h>
}

int main(int argc, char** argv) {

    AVFormatContext *input_context = nullptr;
    AVFormatContext *output_context = nullptr;
    const AVCodec *video_decoder = nullptr;
    AVCodecContext *video_decoder_context = nullptr;
    const AVCodec *audio_decoder = nullptr;
    AVCodecContext *audio_decoder_context = nullptr;
    int fps = 30;
    AVStream *stream = nullptr;
    AVStream *audio_stream = nullptr;
    int64_t pts = 0;
    int64_t audio_pts = 0;

      
    // Open input file
    if (avformat_open_input(&input_context, "res/indigo.mp4", nullptr, nullptr) != 0){
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
    int audio_stream_index = -1;

    for (unsigned int i = 0; i < input_context->nb_streams; i++) {
        if (input_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
        } else if (input_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
        }
    }

    if (video_stream_index == -1) {
        std::cout << "Error: no video stream found in input file" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    if (audio_stream_index == -1) {
        std::cout << "Error: no audio stream found in input file" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    video_decoder = avcodec_find_decoder(input_context->streams[video_stream_index]->codecpar->codec_id);
    video_decoder_context = avcodec_alloc_context3(video_decoder);

    if (avcodec_parameters_to_context(video_decoder_context, input_context->streams[video_stream_index]->codecpar) < 0) {
        std::cout << "Failed to copy codec parameters." << std::endl;
        return 1;
    }

    if (avcodec_open2(video_decoder_context, video_decoder, nullptr) < 0) {
        std::cerr << "Error: could not open codec" << std::endl;
        avcodec_free_context(&video_decoder_context);
        avformat_close_input(&input_context);
        return 1;
    }
    

    AVStream *input_audio_stream = input_context->streams[audio_stream_index];
    AVStream *input_video_stream = input_context->streams[video_stream_index];
    AVRational input_video_framerate = input_video_stream->r_frame_rate;

    std::cout << "Input video framerate: " << input_video_framerate.num << "/" << input_video_framerate.den << std::endl;


    const AVCodec *video_encoder = avcodec_find_encoder(AV_CODEC_ID_H265);
    AVCodecContext *video_encoder_context = avcodec_alloc_context3(video_encoder);

    video_encoder_context->bit_rate = 3000000;
    video_encoder_context->width = video_decoder_context->width;
    video_encoder_context->height = video_decoder_context->height;
    video_encoder_context->pix_fmt = video_encoder->pix_fmts[0];
    video_encoder_context->time_base = input_video_stream->time_base;
    video_encoder_context->framerate = input_video_framerate;
    
    if (avcodec_open2(video_encoder_context, video_encoder, nullptr) < 0) {
        std::cout << "Error: could not open codec for output file" << std::endl;
        avcodec_free_context(&video_decoder_context);
        avcodec_free_context(&video_encoder_context);
        avformat_close_input(&input_context);
        return 1;
    }

    // Allocate output context
    avformat_alloc_output_context2(&output_context, nullptr, nullptr, "res/compressed-indigo.mp4");

    // Add a new stream to the output file
    stream = avformat_new_stream(output_context, video_encoder);
    stream->time_base = input_video_stream->time_base;
    

    AVStream *output_audio_stream = avformat_new_stream(output_context, NULL);
    if (!output_audio_stream) {
        fprintf(stderr, "Could not allocate output audio stream\n");
        return AVERROR(ENOMEM);
    }
    
    // copy the codecpar from the input audio stream to the output audio stream
    avcodec_parameters_copy(output_audio_stream->codecpar, input_audio_stream->codecpar);
    

    // Copy codec parameters from input to output stream
    if (avcodec_parameters_from_context(stream->codecpar, video_encoder_context) < 0) {
        std::cout << "Error copying codec parameters" << std::endl;
        return 1;
    }

    // Open output file
    if (!(output_context->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_context->pb, "res/compressed-indigo.mp4", AVIO_FLAG_WRITE) < 0) {
            std::cout << "Error: could not open output file" << std::endl;
            avcodec_free_context(&video_decoder_context);
            avcodec_free_context(&video_encoder_context);
            avformat_close_input(&input_context);
            return 1;
        }
    }

    // Write the header
    if (avformat_write_header(output_context, nullptr) < 0) {
        std::cout << "Error: could not write header to output file" << std::endl;
        avcodec_free_context(&video_decoder_context);
        avcodec_free_context(&video_encoder_context);
        avio_closep(&output_context->pb);
        avformat_close_input(&input_context);
    }


    AVPacket packet;

    AVFrame *frame = av_frame_alloc();
    AVFrame *audio_frame = av_frame_alloc();
    
    av_frame_get_buffer(frame, 0);
    av_frame_get_buffer(audio_frame, 0);
    

    while (av_read_frame(input_context, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            if (avcodec_send_packet(video_decoder_context, &packet) < 0) {
                std::cout << "Error sending packet" << std::endl;
                break;
            }
            while (int ret = avcodec_receive_frame(video_decoder_context, frame) >= 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    std::cout << "Error returning frame" << std::endl;
                    break;
                }
                if (avcodec_send_frame(video_encoder_context, frame) < 0 ) {
                    std::cerr << "Error sending frame" << std::endl;
                    break;
                }
                while (avcodec_receive_packet(video_encoder_context, &packet) >= 0) {
                    if (av_interleaved_write_frame(output_context, &packet) < 0) {
                        std::cerr << "Error writing to frame to output" << std::endl;
                        break;
                    }
                }
            }
        } else if (packet.stream_index == audio_stream_index) {
            if (av_interleaved_write_frame(output_context, &packet) < 0) {
                fprintf(stderr, "Error writing audio packet to output file\n");
                return AVERROR(EIO);
            }
        }
    }
    av_packet_unref(&packet);


    // write the trailer and close the output file
    av_write_trailer(output_context);
    avio_closep(&output_context->pb);

    // close the decoder and encoder
    avcodec_free_context(&video_encoder_context);
    avcodec_free_context(&video_decoder_context);
    // avcodec_free_context(&audio_encoder_context);
    avcodec_free_context(&audio_decoder_context);

    // close the input file
    avformat_close_input(&input_context);

    return 0;
}