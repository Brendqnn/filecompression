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
    // output context
    if (avformat_alloc_output_context2(&output_context, nullptr, nullptr, "output.mp4") < 0)
    {
        std::cout << "Error creating output context" << std::endl;
        avcodec_close(codec_context);
        avformat_close_input(&input_context);
        return 1;
    }



    return 0;
}