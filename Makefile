CFLAGS = -I C:\ffmpeg\include
LDFLAGS = -L C:\ffmpeg\lib -lavcodec -lavformat -lavutil

all:
	g++ $(CFLAGS) -o main.exe main.cpp $(LDFLAGS) && start main

clean:
	del *.exe