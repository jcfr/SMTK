//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "smtk/mesh/operators/ReadResource.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ResourceItem.h"
#include "smtk/attribute/StringItem.h"
#include "smtk/attribute/VoidItem.h"

#include "smtk/common/FileLocation.h"
#include "smtk/common/Paths.h"

#include "smtk/io/ReadMesh.h"

#include "smtk/mesh/json/jsonResource.h"

#include "smtk/mesh/core/Resource.h"

#include "smtk/mesh/ReadResource_xml.h"
#include "smtk/mesh/operators/Read.h"

#include "smtk/common/CompilerInformation.h"

SMTK_THIRDPARTY_PRE_INCLUDE
#include "nlohmann/json.hpp"
SMTK_THIRDPARTY_POST_INCLUDE

#include <fstream>

namespace smtk
{
namespace mesh
{

ReadResource::Result ReadResource::operateInternal()
{
  std::string filename = this->parameters()->findFile("filename")->value();

  std::ifstream file(filename);
  if (!file.good())
  {
    smtkErrorMacro(log(), "Cannot read file \"" << filename << "\".");
    return this->createResult(smtk::operation::Operation::Outcome::FAILED);
  }

  nlohmann::json j;
  try
  {
    j = nlohmann::json::parse(file);
  }
  catch (...)
  {
    smtkErrorMacro(log(), "Cannot parse file \"" << filename << "\".");
    return this->createResult(smtk::operation::Operation::Outcome::FAILED);
  }

  std::string meshFilename = j.at("Mesh URL");

  auto refDirectory = smtk::common::Paths::directory(filename);
  smtk::common::FileLocation meshFileLocation(meshFilename, refDirectory);

  smtk::io::ReadMesh read;
  smtk::mesh::ResourcePtr resource = smtk::mesh::Resource::create();
  smtk::mesh::from_json(j, resource);
  read(meshFileLocation.absolutePath(), resource);
  resource->setLocation(filename);

  Result result = this->createResult(smtk::operation::Operation::Outcome::SUCCEEDED);

  {
    smtk::attribute::ResourceItem::Ptr created = result->findResource("resource");
    created->setValue(resource);
  }

  return result;
}

const char* ReadResource::xmlDescription() const
{
  return ReadResource_xml;
}

void ReadResource::markModifiedResources(ReadResource::Result& res)
{
  auto resourceItem = res->findResource("resource");
  for (auto rit = resourceItem->begin(); rit != resourceItem->end(); ++rit)
  {
    auto resource = std::dynamic_pointer_cast<smtk::resource::Resource>(*rit);

    // Set the resource as unmodified from its persistent (i.e. on-disk) state
    resource->setClean(true);
  }
}

smtk::resource::ResourcePtr read(const std::string& filename)
{
  ReadResource::Ptr read = ReadResource::create();
  read->parameters()->findFile("filename")->setValue(filename);
  ReadResource::Result result = read->operate();
  if (result->findInt("outcome")->value() != static_cast<int>(ReadResource::Outcome::SUCCEEDED))
  {
    return smtk::resource::ResourcePtr();
  }
  return result->findResource("resource")->value();
}
}
}
