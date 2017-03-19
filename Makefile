CC=g++
CFLAGS=-c -Wall -g -D_FILE_OFFSET_BITS=64
LDFLAGS=-lGL -lGLU -lSDL
IFLAGS=-I/usr/include/SDL -I. -I/usr/share/doc/nvidia-glx-new-dev/include
SOURCES=VisMain.cpp Render.cpp RenderDevice.cpp Input.cpp Globals.cpp State.cpp Vec.cpp RenderBlocks.cpp UI.cpp BlockManager.cpp BlockPriority.cpp Shaders.cpp SubSelect.cpp Keyframes.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=vis

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(IFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(IFLAGS) $(CFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE) *.o *~