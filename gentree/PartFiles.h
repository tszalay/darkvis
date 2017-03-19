#include "Formats.h"

#ifndef _PARTFILES_H_
#define _PARTFILES_H_

#include <stdio.h>
#include <algorithm>

// helper function
inline string getSubfile(string filename, int num)
{
    ostringstream s;
    s << num; 
    return filename + "." + s.str();
}

// no files larger than a bit >2 GB, let's say
#define MAX_FILE_SIZE 1000000000
//#define MAX_FILE_SIZE 30000000
// and this one is only for the reader
#define READ_BUF_SIZE 400000000

template<typename T>
class BufferedReader
{
private:
    string filename;

    // internal file
    FILE *inFile;

    // objects, and # of objects
    T* buffer;
    int bufLength;

    // file stats
    int curFile;
    int curIndex;
    int totalCount;

    int maxFile;

    bool canRead;

    // delete files after reading?
    bool delFiles;



    void init()
    {
	buffer = new T[BufMax];
	bufLength = 0;
	curFile = 0;
	curIndex = 0;
	totalCount = 0;
	maxFile = -1;
	filename = "";
	canRead = true;
	delFiles = false;
	inFile = NULL;
    }


    void cleanup()
    {
	// close + free
	if (inFile != NULL)
	{	    
	    fclose(inFile);
	    inFile = NULL;
	}
	delete[] buffer;
    }
    
    void readFile()
    {
        // close current file
	if (inFile != NULL)
	{
	    fclose(inFile);
	    if (delFiles)
		remove(getSubfile(filename, curFile-1).c_str());
	}
	// trying to read past max file?
	if (maxFile != -1 && curFile > maxFile)
	{
	    canRead = false;
	    inFile = NULL;
	    return;
	}
	inFile = fopen(getSubfile(filename,curFile).c_str(),"rb");
	// next file does not exist
	if (inFile == NULL)
	{
	    canRead = false;
	    return;
	}
    }

    void readBuffer()
    {
	if (inFile == NULL)
	    return;
	
	// only load if we can
	if (!canRead)
	    return;
	
	// read as much as we can
	bufLength = fread(buffer, sizeof(T), BufMax, inFile);
	// advance the file, if we read nothing
	if (bufLength == 0)
	{
	    curFile++;
	    readFile();
	    // and only if the next file exists
	    if (canRead)
		bufLength = fread(buffer, sizeof(T), BufMax, inFile);
	}

	curIndex = 0;
    }

    // mark as private, no copying allowed
    BufferedReader(const BufferedReader& other);

    // same here
    BufferedReader& operator=(const BufferedReader& old);

public:

    static const int BufMax = READ_BUF_SIZE/sizeof(T);

    BufferedReader()
    {
	init();
	canRead = false;
    }

    BufferedReader(string fname)
    {
	init();
	filename = fname;
	// read first file
	readFile();
	// and fill buffer
	readBuffer();
    }

    BufferedReader(string fname, int startfile, int maxfile)
    {
	init();
	filename = fname;
	maxFile = maxfile;
	curFile = startfile;
	// read first file
	readFile();
	// and fill buffer
	readBuffer();
    }

    ~BufferedReader()
    {
	cleanup();
    }
    
    T Read()
    {
	return buffer[curIndex];	
    }

    T* GetPointer()
    {
	return buffer + curIndex;
    }

    bool Next()
    {
	totalCount++;
	curIndex++;
	// end of this buffer/file?
	if (curIndex >= bufLength)
	    readBuffer();

	return canRead;
    }

    void SetDelete(bool val)
    {
	delFiles = val;
    }

    bool CanRead()
    {
	return canRead;
    }
};


template<typename T>
class RawWriter
{
private:
    string filename;

    // internal file
    FILE *outFile;

    // file stats
    int curFile;
    int curCount;


    void init()
    {
	curFile = 0;
	curCount = 0;
	filename = "";
	outFile = NULL;
    }


    void cleanup()
    {
	// close
	if (outFile != NULL)
	{
	    fclose(outFile);
	    outFile = NULL;
	}
    }
    
    void nextFile()
    {
        // close current file
	if (outFile != NULL)
	    fclose(outFile);
	outFile = fopen(getSubfile(filename,curFile).c_str(),"wb");
	curCount = 0;
	curFile++;
    }

    // mark as private, no copying allowed
    RawWriter(const RawWriter& other);

    // same here
    RawWriter& operator=(const RawWriter& old);


public:

    static const int BufMax = MAX_FILE_SIZE/sizeof(T);

    RawWriter(string fname)
    {
	init();
	filename = fname;
	// open first file
	nextFile();
    }

    RawWriter(string fname, int startfile)
    {
	init();
	filename = fname;
	curFile = startfile;
	// open first file
	nextFile();
    }

    ~RawWriter()
    {
	cleanup();
    }

    void Close()
    {
	cleanup();
    }
    
    void Write(T t)
    {
	// filled up file?
	if (curCount >=  BufMax)
	    nextFile();
	fwrite(&t, sizeof(T), 1, outFile);
	curCount++;
    }
};

template<typename T>
class BufferedWriter
{
private:
    string filename;

    // objects, and # of objects
    T* buffer;

    // file stats
    int curFile;
    int curCount;

    // do we sort?
    bool doSort;
    
    void init()
    {
	buffer = new T[BufMax];
	curFile = 0;
	curCount = 0;
	filename = "";
	doSort = false;
    }


    void cleanup()
    {
	// free data
	if (buffer != NULL)
	{
	    delete[] buffer;
	    buffer = NULL;
	}
    }
    
    void writeBuffer()
    {	
	if (curCount == 0)
	    return;
        // sort (?) and write buffer
	if (doSort)
	    std::sort(buffer, buffer+curCount);

	// open file for writing
	FILE *outFile = fopen(getSubfile(filename,curFile).c_str(),"wb");
	// write
	fwrite(buffer, sizeof(T), curCount, outFile);
	curCount = 0;
	// and close
	fclose(outFile);
	// next file
	curFile++;
    }

    // mark as private, no copying allowed
    BufferedWriter(const BufferedWriter& other);

    // same here
    BufferedWriter& operator=(const BufferedWriter& old);


public:

    static const int BufMax = MAX_FILE_SIZE/sizeof(T);

    BufferedWriter(string fname)
    {
	init();
	filename = fname;
    }

    BufferedWriter(string fname, int startfile)
    {
	init();
	filename = fname;
	curFile = startfile;
    }

    ~BufferedWriter()
    {
	writeBuffer();
	cleanup();
    }
    
    void Write(T t)
    {
	// write element
	buffer[curCount] = t;
	curCount++;

	// filled up buffer?
	if (curCount >= BufMax)
	    writeBuffer();
    }

    void SetSort(bool val)
    {
	doSort = val;
    }

    int Close()
    {	
        // save before closing
	writeBuffer();
	cleanup();
	return curFile;
    }
};

// This is a class for a cross-platform file writer
// capable of writing large files
class LargeWriter
{
private:

    uint64_t writeLocation;
    FILE *file;

    void init()
    {
	file = NULL;
	writeLocation = 0;
    }

    // mark as private, no copying allowed
    LargeWriter(const LargeWriter& other);

    // same here
    LargeWriter& operator=(const LargeWriter& old);


public:

    LargeWriter(string filename)
    {
	init();
	file = fopen(filename.c_str(), "wb");
    }

    ~LargeWriter()
    {
	Close();
    }

    void Write(void * data, int size)
    {
	fwrite(data, size, 1, file);
	writeLocation += size;
    }

    uint64_t GetLocation()
    {
	return writeLocation;
    }

    void Close()
    {
	if (file != NULL)
	{
	    fclose(file);
	    file = NULL;
	}
    }
};


#endif
