#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>


extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h>
}



int main(int argc, char** argv) {

    AVFormatContext *input_context = nullptr;
    AVFormatContext *output_context = nullptr;
    AVCodec *codec = nullptr;
    AVStream *stream = nullptr;
    AVCodecContext *codec_context = nullptr;
    AVDictionary *options = NULL;

    if (avformat_open_input(&input_context, "input.mp4", nullptr, nullptr) != 0)
    {
        std::cout << "Error opening input file" << std::endl;
        return 1;
    }

    if (avformat_find_stream_info(input_context, nullptr) < 0)
    {
        std::cout << "Error finding stream info" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    int video_stream_index = av_find_best_stream(input_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    if (video_stream_index < 0)
    {
        std::cout << "Error finding video stream" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    if (!codec)
    {
        std::cout << "codec not found" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }
    
    // get pointer to the codec context for the video stream
    codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(codec_context, input_context->streams[video_stream_index]->codecpar);

    // open codec
    if (avcodec_open2(codec_context, codec, nullptr) < 0)
    {
        std::cout << "Erorr Opening codec" << std::endl;
        avformat_close_input(&input_context);
        return 1;
    }

    // setting bitrate
    av_dict_set(&options, "b:v", "1000000", 0);
    avcodec_open2(codec_context, codec, &options);
    
    // output context
    if (avformat_alloc_output_context2(&output_context, nullptr, nullptr, "output.mp4") < 0)
    {
        std::cout << "Error creating output context" << std::endl;
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }
    // add stream to output context 
    stream = avformat_new_stream(output_context, codec);
    if (!stream)
    {
        std::cout << "Error creating stream" << std::endl;
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    // copy codec to output file
    if (avcodec_parameters_from_context(stream->codecpar, codec_context)< 0)
    {
        std::cout << "Error copying codec context" << std::endl;
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }
    
    if (avio_open(&output_context->pb, "output.mp4", AVIO_FLAG_WRITE) < 0)
    {
        std::cout << "Error opening output file" << std::endl;
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    if (avformat_write_header(output_context, nullptr) < 0)
    {
        std::cout << "Error writing header" << std::endl;
        avio_closep(&output_context->pb);
        avformat_free_context(output_context);
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }

    // compress video frame
    AVPacket packet;
    av_init_packet(&packet);
    while (av_read_frame(input_context, &packet) >= 0)
    {
        if(packet.stream_index == video_stream_index)
        {
            AVFrame *frame = av_frame_alloc();
            int ret;
            ret = avcodec_send_frame(codec_context, frame);
            if (ret < 0)
            {
                std::cout << "Error sending packet" << std::endl;
                av_frame_free(&frame);
                av_packet_unref(&packet);
                av_write_trailer(output_context);
                avio_closep(&output_context->pb);
                avformat_free_context(output_context);
                avcodec_close(codec_context);
                avformat_close_input(&input_context);
                return 1;
            }
            AVPacket packet_compress;
            av_init_packet(&packet_compress);
            ret = avcodec_receive_packet(codec_context, &packet_compress);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cout << "Error no packet received" << std::endl;
            }  else {
                packet_compress.stream_index = stream->index;
                if (av_interleaved_write_frame(output_context, &packet_compress) < 0) 
                {
                    std::cout << "Error writing packet" << std::endl;
                }

                av_packet_unref(&packet_compress);
            }
        }
        
    }    

    return 0;
}