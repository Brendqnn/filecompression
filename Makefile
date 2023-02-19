CFLAGS = -I C:\sdk\ffmpeg\include -I include 
LDFLAGS = -L C:\sdk\ffmpeg\lib -lavcodec -lavformat -lavutil -lswresample

all:
	g++ $(CFLAGS) -o main.exe src/*.cpp main.cpp $(LDFLAGS)

clean:
	del *.exe
