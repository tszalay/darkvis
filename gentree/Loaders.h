#ifndef _LOADERS_H_
#define _LOADERS_H_

#include <stdio.h>
#include "Formats.h"
#include "Gadget.h"


HsmlData LoadHsml(PathInfo& ps, int id, int file);
void FreeHsml(HsmlData& data);

VertexData LoadSnap(PathInfo& ps, int id, int file);
void FreeSnap(VertexData& data);

VertexData LoadSnapHeader(PathInfo& ps, int id, int file);

GroupData LoadGroup(PathInfo& ps, int id, int file, bool vel);
void FreeGroup(GroupData& data);

GroupData LoadGroupHeader(PathInfo& ps, int id, int file, bool vel);

IDData LoadIDs(PathInfo& ps, int id, int file);
void FreeIDs(IDData& data);

TreeData LoadTree(string filename);
void FreeTree(TreeData& data);

#endif
