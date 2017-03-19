CC=g++
CCFLAGS = -D_FILE_OFFSET_BITS=64 -ggdb
LDFLAGS=
IFLAGS=-I.
SOURCES=Loaders.cpp Interleave.cpp BuildIndex.cpp Main.cpp TreeIndex.cpp CreateBlocks.cpp WriteBlock.cpp HaloTable.cpp MergeBlock.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=createblocks

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CCFLAGS) $(IFLAGS) $(LDFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(EXECUTABLE) *.o *~