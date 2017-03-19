#include "Formats.h"
#include "PartFiles.h"
#include "CreateBlocks.h"

#ifndef _PROCESS_H_
#define _PROCESS_H_

SnapHeader Interleave(PathInfo& ps, string filename, int snap);
BlockFile BuildIndex(SnapHeader *curSnap, SnapHeader *nextSnap, string filename);
void ProcessBlocks(string infile, string outfile, int maxcnt, int numfiles, BlockFile& bf);

void BuildSubOrder(PathInfo& ps, int snap, string filename);
void PrepareSubIds(PathInfo& ps, int snap, string filename);
void SequenceSubIds(PathPair paths, string suborder, string filename);
void BuildHaloTable(PathInfo& ps, string treefile, PathPair paths, int snap, int step, string filename);

#endif
