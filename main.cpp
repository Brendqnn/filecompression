#include <iostream>
#include "slcompressor.hpp"



int main(int argc, char** argv) {
    SLcompressor s;

    s.filename = "res/indigo.mp4";
    std::cout << s.filename << std::endl;

    s.input_context = nullptr;
    s.output_context = nullptr;

    s.open_input_media();
    s.find_media_stream();
    s.set_input_stream();
    s.open_decoder_context();
    s.open_encoder_context();
    s.copy_audio_parameters();
    s.write_file_header();
    s.read_frames();
    s.close_resources();

    return 0;
    
}