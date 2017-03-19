#include "Formats.h"
#include <string>
#include <stdio.h>

struct OldFormat
{
    int snapnum; // number of snapshot
    int unused;
    double time; // time of snapshot, as log(a)
    uint64_t firstLocation; // file location of first block
    uint64_t firstLength; // file length of first block
    int firstFile; // subfile containing first block
    int unused2;
    double pos[3]; // min. of snapshot in each dim
    double scale[3]; // size of snapshot in each dim

};

struct OldFormat2
{
    int snapnum; // number of snapshot
    double time; // time of snapshot, as log(a)
    uint64_t firstLocation; // file location of first block
    uint64_t firstLength; // file length of first block
    int firstFile; // subfile containing first block
    double pos[3]; // min. of snapshot in each dim
    double scale[3]; // size of snapshot in each dim

};

int main()
{
    string base = "/media/disk1/aq3-opa/blocks_";
    for (int i=255; i<=255; i+=4)
    {
	OldFormat fold;

	FILE *file = fopen((base + toString<int>(i) + 
"_info").c_str(),"rb");
	if (file == NULL)
		continue;
	fread(&fold, sizeof(fold), 1, file);
	fclose(file);

	BlockFile bf;

	bf.snapnum = fold.snapnum;
	bf.firstFile = fold.firstFile;
	bf.firstLocation = fold.firstLocation;
	bf.firstLength = fold.firstLength;
	bf.time = fold.time;
	for (int j=0;j<3;j++)
	{
	    bf.scale[j] = fold.scale[j];
	    bf.pos[j] = fold.pos[j];
	}

	file = fopen((base + toString<int>(i) + "_info").c_str(),"wb");
	fwrite(&bf, sizeof(bf), 1, file);
	fclose(file);
    }

    return 0;
}
