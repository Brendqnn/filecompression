#include <iostream>
#include "slcompressor.hpp"



int main(int argc, char** argv) {

    SLcompressor s;

    s.filename = "res/indigo.mp4";
    std::cout << s.filename << std::endl;

    s.open_input_media();
    
    return 0;
    
}