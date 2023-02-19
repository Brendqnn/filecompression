#include "slcompressor.hpp"
#include <iostream>


void SLcompressor::open_input_media() {
    if (avformat_open_input(&input_context, filename, nullptr, nullptr) < 0) {
        std::cout << "Error opening input file" << std::endl;
    }
}

void SLcompressor::find_media_stream() {
    if (avformat_find_stream_info(input_context, nullptr) < 0) {
        std::cout << "Error finding stream info" << std::endl;
        avformat_close_input(&input_context);
    }
    
    video_stream_index = -1;
    audio_stream_index = -1;
    
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
    }

    if (audio_stream_index == -1) {
        std::cout << "Error: no audio stream found in input file" << std::endl;
        avformat_close_input(&input_context);
    }
}

void SLcompressor::open_decoder_context() {
    video_decoder = avcodec_find_decoder(input_context->streams[video_stream_index]->codecpar->codec_id);
    video_decoder_context = avcodec_alloc_context3(video_decoder);
    if (avcodec_parameters_to_context(video_decoder_context, input_context->streams[video_stream_index]->codecpar) < 0) {
        std::cout << "Failed to copy codec parameters." << std::endl;
    }
    if (avcodec_open2(video_decoder_context, video_decoder, nullptr) < 0) {
        std::cerr << "Error: could not open codec" << std::endl;
        avcodec_free_context(&video_decoder_context);
        avformat_close_input(&input_context);
    }
}

void SLcompressor::set_stream() {
    input_audio_stream = input_context->streams[audio_stream_index];
    input_video_stream = input_context->streams[video_stream_index];
    input_video_framerate = input_video_stream->r_frame_rate;
}

void SLcompressor::alloc_output_context() {
    avformat_alloc_output_context2(&output_context, nullptr, nullptr, "res/compressed-indigo.mp4");
    output_video_stream = avformat_new_stream(output_context, video_encoder);
    output_video_stream->time_base = input_video_stream->time_base;
}

void SLcompressor::open_encoder_context() {
    const AVCodec *video_encoder = avcodec_find_encoder(AV_CODEC_ID_H265);
    AVCodecContext *video_encoder_context = avcodec_alloc_context3(video_encoder);
    if (avcodec_open2(video_encoder_context, video_encoder, nullptr) < 0) {
        std::cout << "Error: could not open codec for output file" << std::endl;
        avcodec_free_context(&video_decoder_context);
        avcodec_free_context(&video_encoder_context);
        avformat_close_input(&input_context);
    }
    if (avcodec_parameters_from_context(output_video_stream->codecpar, video_encoder_context) < 0) {
        std::cout << "Error copying codec parameters" << std::endl;
    }
}

void SLcompressor::set_encoder_properties() {
    video_encoder_context->bit_rate = 3000000;
    video_encoder_context->width = video_decoder_context->width;
    video_encoder_context->height = video_decoder_context->height;
    video_encoder_context->pix_fmt = video_encoder->pix_fmts[0];
    video_encoder_context->time_base = input_video_stream->time_base;
    video_encoder_context->framerate = input_video_framerate;
}

void SLcompressor::copy_audio_parameters() {
    output_audio_stream = avformat_new_stream(output_context, NULL);
    if (!output_audio_stream) {
        std::cout << "no audio stream found" << std::endl;
    }
    avcodec_parameters_copy(output_audio_stream->codecpar, input_audio_stream->codecpar);
}

void SLcompressor::open_output_media() {
    if (!(output_context->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_context->pb, "res/compressed-indigo.mp4", AVIO_FLAG_WRITE) < 0) {
            std::cout << "Error: could not open output file" << std::endl;
            avcodec_free_context(&video_decoder_context);
            avcodec_free_context(&video_encoder_context);
            avformat_close_input(&input_context);
        }
    }
}

void SLcompressor::write_file_header() {
    if (avformat_write_header(output_context, nullptr) < 0) {
        std::cout << "Error: could not write header to output file" << std::endl;
        avcodec_free_context(&video_decoder_context);
        avcodec_free_context(&video_encoder_context);
        avio_closep(&output_context->pb);
        avformat_close_input(&input_context);
    }
}

void SLcompressor::read_frames() {
    frame = av_frame_alloc();
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
                std::cout << "Error writing audio packet to output file" << std::endl;
            }
        }
        av_packet_unref(&packet);
    }
    av_write_trailer(output_context);
}

void SLcompressor::close_resources() {
    avio_closep(&output_context->pb);
    avcodec_free_context(&video_encoder_context);
    avcodec_free_context(&video_decoder_context);
    avformat_close_input(&input_context);
}






