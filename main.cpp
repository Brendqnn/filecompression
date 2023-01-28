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
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    AVStream *stream = nullptr;
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

    int video_stream_index = av_find_best_stream(input_context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    
    // codec_context->codec_id = AV_CODEC_ID_VP9;
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context->width = input_context->streams[video_stream_index]->codecpar->width;
    codec_context->height = input_context->streams[video_stream_index]->codecpar->height;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->bit_rate = 1000000;
    codec_context->gop_size = 12;
    codec_context->max_b_frames = 0;
    codec_context->time_base = (AVRational){1, fps};
    
    std::cout << "Codec name: " << avcodec_get_name(codec_context->codec_id) << std::endl;

    // open codec
    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        std::cout << "Error opening codec" << std::endl;
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

    AVCodecParameters *input_codec_params = input_context->streams[video_stream_index]->codecpar;

    // Copy the codec parameters from the input stream to the output stream
    if (avcodec_parameters_from_context(stream->codecpar, codec_context) < 0) {
        std::cout << "Error copying codec parameters" << std::endl;
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

    frame->time_base = codec_context->time_base;
    frame->width = codec_context->width;
    frame->height = codec_context->height;
    frame->format = codec_context->pix_fmt;
    
    if (av_frame_get_buffer(frame, 32)) {
        std::cout << "Error allocating frame buffer" << std::endl;
        return 1;
    } 

    const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    AVCodecContext *decoder_context = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoder_context, input_codec_params);
    
    if (!decoder_context) {
        std::cout << "Error allocating decoder context" << std::endl;
        return 1;
    }
    

    if (avcodec_open2(decoder_context, decoder, nullptr) < 0) {
        std::cout << "Error opeing codec" << std::endl;
        return 1;
    }

    
    // read and compress video frames
    while (av_read_frame(input_context, &packet) >= 0) {
    // check if the packet belongs to the video stream
    if (packet.stream_index == video_stream_index) {
        // send the packet to the decoder
        if (avcodec_send_packet(decoder_context, &packet) < 0) {
            std::cout << "Error sending packet to decoder" << std::endl;
            break;
        }

        // receive decoded frames from the decoder
        while (avcodec_receive_frame(decoder_context, frame) >= 0) {
            // convert the frame to the desired format
            // send the frame to the encoder
            if (avcodec_send_frame(codec_context, frame) < 0) {
                std::cout << "Error sending frame to encoder" << std::endl;
                break;
            }

            // receive encoded packets from the encoder
            while (avcodec_receive_packet(codec_context, &packet) >= 0) {
                // write the packet to the output file
                if (av_interleaved_write_frame(output_context, &packet) < 0) {
                    std::cout << "Error writing packet" << std::endl;
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
    avcodec_free_context(&decoder_context);
    avcodec_free_context(&codec_context);

    // close the input file
    avformat_close_input(&input_context);

    return 0;


}