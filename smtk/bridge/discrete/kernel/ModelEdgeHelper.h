//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

// .NAME MODELEDGEHELPER - Helper Classes for creating model edges
// .SECTION Description
// Includes NewModelEdgeInfo and LoopInfo
#ifndef __smtkdiscrete_MODELEDGEHELPER_H
#define __smtkdiscrete_MODELEDGEHELPER_H

#include <vtkIdList.h>
#include <vtksys/MD5.h>

#include "vtkIdTypeArray.h"
#include "vtkSmartPointer.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

class NewModelEdgeInfo
{
public:
  NewModelEdgeInfo()
    : currentModelEdgeId(0)
    , taggedIndex(-1)
    , numberOfNewEdgesSinceTag(0)
  {
  }
  const std::string& GetTaggedFaceInfo() const { return this->taggedFaceInfo; }
  const std::string& GetCurrentFaceInfo() const { return this->currentFaceInfo; }
  vtkIdType GetCurrentModelEdgeId() const { return this->currentModelEdgeId; }
  // By clearing the faceInfomation we will force a
  // new model edge to be defined even if it bounds the same set
  // of model faces the previous model edge did - this is deal with the
  // issue of a set model edges (that don't yet exist) that bound the same
  // model faces but are seperated by existing model edges
  void ClearCurrentFaceInfo() { this->currentFaceInfo = ""; }
  //By clearing the tagged face information we force the object to record
  // the face information of the next edge that gets inserted.  This is
  // needed when determining if we need to "stich" together 2 model edge segments
  void ClearTaggedFaceInfo();
  void Reset();
  // Returns the id of the model edge that owns the mesh edge
  // Note that all model edge ids are negative to indicate they need to be
  // replaced by valid Ids
  vtkIdType InsertMeshEdge(vtkIdType edgeId, const std::string& faceInfo);

  //See if we need to move the mesh edges classified on the same model edge as the
  // tagged mesh edge to the end of the vector - this is to deal
  // with the issue of a "model edge" not starting at the proper
  // "model vertex" - the method then returns the model edge id
  // that should be "deleted"
  bool CheckLastModelEdge(vtkIdType& gedge);

  static std::string to_key(const vtkIdType* data, vtkIdType len);
  static std::string to_key(vtkIdList* list);

  std::vector<std::pair<vtkIdType, vtkIdType> > info;

protected:
  std::string taggedFaceInfo;
  std::string currentFaceInfo;
  vtkIdType currentModelEdgeId, taggedIndex, numberOfNewEdgesSinceTag;
};

class LoopInfo
{
public:
  LoopInfo() {}
  void Reset() { loop.clear(); }

  //Insert a model edge only if it is not the same as the last one inserted
  void InsertModelEdge(vtkIdType edgeId, bool orientation);
  //Remove a model edge from the loop
  void RemoveModelEdge(vtkIdType gedge);

  std::vector<std::pair<vtkIdType, bool> > loop;
};

class FaceEdgeSplitInfo
{
public:
  FaceEdgeSplitInfo() { this->Reset(); }

  void Reset();
  void DeepCopy(FaceEdgeSplitInfo& splitInfo);

  // Description:
  // The map of <OldEdgeId, NewVertId, NewEdgId>
  // This list contains all the edges that are being split and the new verts
  // and edges that are being created by edge-split.
  // The New Ids should match between server and client
  vtkSmartPointer<vtkIdTypeArray> SplitEdgeVertIds;

  // Description:
  // The map of <NewEdgeId, VertId1, VertId2>
  // These Edges are brand new, meaning NOT includes those generated by
  // splitting existing edges. The verts could be existing or new, and
  // the two verts could be the same vertex.
  vtkSmartPointer<vtkIdTypeArray> CreatedModelEdgeVertIDs;

  // Description:
  // The map of <faceId, nLoops, [nEdges, (gedges[n]...), (orientations[n]...)]>
  vtkSmartPointer<vtkIdTypeArray> FaceEdgeLoopIDs;
};

void inline NewModelEdgeInfo::ClearTaggedFaceInfo()
{
  this->taggedFaceInfo = "";
  this->numberOfNewEdgesSinceTag = 0;
}

void inline NewModelEdgeInfo::Reset()
{
  this->info.clear();
  this->currentFaceInfo = "";
  this->taggedFaceInfo = "";
  this->currentModelEdgeId = 0;
  this->taggedIndex = -1;
  this->numberOfNewEdgesSinceTag = 0;
}

// Returns the id of the model edge that owns the mesh edge
// Note that all model edge ids are negative to indicate they need to be
// replaced by valid Ids
inline vtkIdType NewModelEdgeInfo::InsertMeshEdge(vtkIdType edgeId, const std::string& faceInfo)
{
  if (!info.size())
  {
    this->taggedFaceInfo = faceInfo;
    this->currentFaceInfo = faceInfo;
    this->currentModelEdgeId--;
    this->taggedIndex = 0;
    this->numberOfNewEdgesSinceTag = 1;
  }
  else if (this->currentFaceInfo != faceInfo)
  {
    // This is a new model edge
    this->currentModelEdgeId--;
    this->currentFaceInfo = faceInfo;
    ++this->numberOfNewEdgesSinceTag;
  }

  if (this->taggedFaceInfo == "")
  {
    this->taggedFaceInfo = faceInfo;
    this->taggedIndex = this->info.size();
    this->numberOfNewEdgesSinceTag = 1;
  }

  this->info.push_back(std::pair<vtkIdType, vtkIdType>(edgeId, this->currentModelEdgeId));
  return this->currentModelEdgeId;
}

inline bool NewModelEdgeInfo::CheckLastModelEdge(vtkIdType& gedge)
{
  // Check to see if the taggedFaceInfo matches the current one - if it
  // doesn't then we don't need to do anything since those model edges
  // bound different sets of model faces or if we were told we needed to create
  // a new model edge with the next insertion (info is "")
  if ((this->taggedFaceInfo != this->currentFaceInfo) || (this->currentFaceInfo == ""))
  {
    return false;
  }

  // Was there only one edge being created?
  if (this->numberOfNewEdgesSinceTag == 1)
  {
    return false;
  }
  // std::cout << "Reassembling Model Edge!!\n";
  // OK we need to move things arround - first we need to copy the tagged edge out of
  // the array
  std::vector<std::pair<vtkIdType, vtkIdType> >::iterator startI, endI, iter;
  startI = this->info.begin() + this->taggedIndex;
  endI = startI;
  gedge = startI->second;
  for (endI = startI; (endI->second == gedge) && (endI != info.end()); endI++)
    ;
  if (endI == this->info.end())
  {
    std::cout << "Problem finding end of edge!!\n";
    return false;
  }
  std::vector<std::pair<vtkIdType, vtkIdType> > temp;
  temp.insert(temp.begin(), startI, endI);
  this->info.erase(startI, endI);
  // OK we've isolated the edge fragment - now reset the model edge to be the
  // surviver
  for (iter = temp.begin(); iter != temp.end(); iter++)
  {
    iter->second = this->currentModelEdgeId;
  }
  // Finally add the fragment to the rest of the model edge
  this->info.insert(this->info.end(), temp.begin(), temp.end());
  return true;
}

inline std::string NewModelEdgeInfo::to_key(vtkIdList* list)
{
  return to_key(list->GetPointer(0), list->GetNumberOfIds());
}

inline std::string NewModelEdgeInfo::to_key(const vtkIdType* data, vtkIdType len)
{
  //sort
  std::vector<vtkIdType> sorted(data, data + len);
  std::sort(sorted.begin(), sorted.end());

  //hash the raw memory of the vector as a string we have to give it
  //a length or else the method will try to do strlen which will be very bad.
  //md5 really doesn't care if what type the data is anyhow
  vtksysMD5* hasher = vtksysMD5_New();
  vtksysMD5_Initialize(hasher);

  //sizeof returns the number of character per vtkIdType and mult be len
  const std::size_t charLen = sizeof(vtkIdType) * len;

  unsigned const char* cdata = reinterpret_cast<unsigned const char*>(&sorted[0]);
  vtksysMD5_Append(hasher, cdata, static_cast<int>(charLen));

  //convert the hash to a string and return
  char hash[33];
  vtksysMD5_FinalizeHex(hasher, hash);
  vtksysMD5_Delete(hasher);
  hash[32] = 0;
  return std::string(hash);
}

//Insert a model edge only if it is not the same as the last one inserted
inline void LoopInfo::InsertModelEdge(vtkIdType edgeId, bool orientation)
{
  std::size_t n = loop.size();
  if (!(n && (loop[n - 1].first == edgeId) && (loop[n - 1].second == orientation)))
  {
    loop.push_back(std::pair<vtkIdType, bool>(edgeId, orientation));
  }
}

inline void LoopInfo::RemoveModelEdge(vtkIdType gedge)
{
  std::vector<std::pair<vtkIdType, bool> >::iterator iter;
  for (iter = this->loop.begin(); !(iter == this->loop.end() || (iter->first == gedge)); iter++)
    ;
  if (iter == this->loop.end())
  {
    std::cout << "Could not find model edge to be removed!\n";
    return;
  }

  this->loop.erase(iter);
}

inline void FaceEdgeSplitInfo::Reset()
{
  this->SplitEdgeVertIds = vtkSmartPointer<vtkIdTypeArray>::New();
  this->CreatedModelEdgeVertIDs = vtkSmartPointer<vtkIdTypeArray>::New();
  this->FaceEdgeLoopIDs = vtkSmartPointer<vtkIdTypeArray>::New();

  this->SplitEdgeVertIds->SetNumberOfComponents(3);
  this->SplitEdgeVertIds->SetNumberOfTuples(0);
  this->CreatedModelEdgeVertIDs->SetNumberOfComponents(3);
  this->CreatedModelEdgeVertIDs->SetNumberOfTuples(0);
  this->FaceEdgeLoopIDs->SetNumberOfComponents(1);
  this->FaceEdgeLoopIDs->SetNumberOfTuples(0);
}

inline void FaceEdgeSplitInfo::DeepCopy(FaceEdgeSplitInfo& splitInfo)
{
  this->Reset();
  this->SplitEdgeVertIds->DeepCopy(splitInfo.SplitEdgeVertIds.GetPointer());
  this->CreatedModelEdgeVertIDs->DeepCopy(splitInfo.CreatedModelEdgeVertIDs.GetPointer());
  this->FaceEdgeLoopIDs->DeepCopy(splitInfo.FaceEdgeLoopIDs.GetPointer());
}

#endif
