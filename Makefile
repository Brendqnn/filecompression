CFLAGS = -I C:\ffmpeg\include
LDFLAGS = -L C:\ffmpeg\lib -lavcodec -lavformat -lavutil -lswresample

all:
	g++ $(CFLAGS) -o main.exe main.cpp $(LDFLAGS)

clean:
	del *.exe