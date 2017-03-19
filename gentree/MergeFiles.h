#include <stdio.h>
#include "PartFiles.h"

#ifndef _MERGEFILES_H_
#define _MERGEFILES_H_

template<typename T>
void MergeFiles(string infile, string outfile, int start1, int end1, int start2, int end2);


/* Merges a bunch of files sorted individually into a bunch of files
   sorted together (but still in the same-sized files), using given comparison function.
   Can use a second temporary location/file, and returns final path. */

template<typename T>
PathPair MergeSorted(string filename, PathPair paths)
{
    int numFiles;

    // get number of files
    FILE *fhead = fopen((paths.location+filename).c_str(), "r");
    fread(&numFiles, sizeof(int), 1, fhead);
    fclose(fhead);
    // and remove that temp file
    remove((paths.location+filename).c_str());

    // return if we don't need ta do nothin
    if (numFiles == 1)
	return paths;

    int numBlocks = numFiles; // number of sorted chunks
    int blockLen = 1; // number of sorted files in each block

    printf("Blocks left: %d...",numBlocks);
    fflush(stdout);

    while (numBlocks > 1)
    {
	for (int start = 0; start < numFiles; start += 2*blockLen)
	{	  
	    if (numFiles - start <= blockLen)
	    {
		// we have reached the end, and only sorted files remain
		// so we do a dummy write to copy data
		MergeFiles<T>(paths.location + filename, paths.temp + filename, start, numFiles-1, numFiles, numFiles);
	    }
	    else if (numFiles - start < 2*blockLen)
	    {
		// we have reached the end, but don't have full set
		MergeFiles<T>(paths.location + filename, paths.temp + filename, start, start+blockLen-1, start+blockLen, numFiles-1);
		numBlocks--;
	    }
	    else
	    {
		// have full set
		MergeFiles<T>(paths.location + filename, paths.temp + filename, start, start+blockLen-1, start+blockLen, start+2*blockLen-1);
		numBlocks--;
	    }
	    printf("%d blocks left...",numBlocks);
	    fflush(stdout);
	}
	blockLen *= 2;
	// swap source and destination reading
	paths.swap();
    }
    printf("done.\n");
    // this is our last written-to destination
    return paths;
}


/* Merges one pair/set of files. */

template<typename T>
void MergeFiles(string infile, string outfile, int start1, int end1, int start2, int end2)
{
//    printf("Merging %s -> %s, (%d, %d) + (%d, %d)\n",infile.c_str(),outfile.c_str(), start1,end1,start2,end2);

    BufferedReader<T> reader1(infile, start1, end1);
    BufferedReader<T> reader2(infile, start2, end2);

    RawWriter<T> writer(outfile, start1);

    // tells them to delete each file as it's read
    reader1.SetDelete(true);
    reader2.SetDelete(true);

    while (reader1.CanRead() && reader2.CanRead())
    {
	// write smaller of the two, using built-in operator
	if (reader1.Read() < reader2.Read())
	{
	    writer.Write(reader1.Read());
	    reader1.Next();
	}
	else
	{
	    writer.Write(reader2.Read());
	    reader2.Next();
	}
    }

    // write any leftovers
    while (reader1.CanRead())
    {
	writer.Write(reader1.Read());
	reader1.Next();
    }
    while (reader2.CanRead())
    {
	writer.Write(reader2.Read());
	reader2.Next();
    }

    writer.Close();

    // print starts of blocks and block size
    printf("(%d,%d,%d)...", start1, start2, 1+(end1-start1));
    fflush(stdout);
}

#endif
