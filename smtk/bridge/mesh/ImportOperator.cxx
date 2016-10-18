//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/bridge/mesh/ImportOperator.h"

#include "smtk/bridge/mesh/Session.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ModelEntityItem.h"
#include "smtk/attribute/StringItem.h"

#include "smtk/io/ImportMesh.h"

#include "smtk/model/Group.h"
#include "smtk/model/Manager.h"
#include "smtk/model/Model.h"

#include "smtk/common/CompilerInformation.h"

using namespace smtk::model;
using namespace smtk::common;

namespace smtk {
namespace bridge {
namespace mesh {

smtk::model::OperatorResult ImportOperator::operateInternal()
{
  // Get the read file name
  smtk::attribute::FileItem::Ptr filePathItem =
    this->specification()->findFile("filename");
  std::string filePath = filePathItem->value();

  smtk::attribute::StringItem::Ptr labelItem =
    this->specification()->findString("label");
  std::string label = labelItem->value();

  // Get the collection from the file
  smtk::mesh::CollectionPtr collection =
    smtk::io::importMesh(filePath, this->activeSession()->meshManager(), label);

  if (!collection || !collection->isValid())
    {
    // The file was not correctly read.
    return this->createResult(smtk::model::OPERATION_FAILED);
    }

  // Assign its model manager to the one associated with this session
  collection->setModelManager(this->activeSession()->manager());

  // Construct the topology
  this->activeSession()->addTopology(std::move(Topology(collection)));

  // Our collections will already have a UUID, so here we create a model given
  // the model manager and uuid
  smtk::model::Model model =
    smtk::model::EntityRef(this->activeSession()->manager(),
                           collection->entity());

  collection->associateToModel(model.entity());

  // Set the model's session to point to the current session
  model.setSession(smtk::model::SessionRef(this->activeSession()->manager(),
                                           this->activeSession()->sessionId()));

  // If we don't call "transcribe" ourselves, it never gets called.
  this->activeSession()->transcribe(
    model, smtk::model::SESSION_EVERYTHING, false);

  smtk::model::OperatorResult result =
    this->createResult(smtk::model::OPERATION_SUCCEEDED);

  smtk::attribute::ModelEntityItem::Ptr resultModels =
    result->findModelEntity("model");
  resultModels->setValue(model);

  smtk::attribute::ModelEntityItem::Ptr created =
    result->findModelEntity("created");
  created->setNumberOfValues(1);
  created->setValue(model);
  created->setIsEnabled(true);

  result->findModelEntity("mesh_created")->setValue(model);

  return result;
}

} // namespace mesh
} //namespace bridge
} // namespace smtk

#include "smtk/bridge/mesh/ImportOperator_xml.h"
#include "smtk/bridge/mesh/Exports.h"

smtkImplementsModelOperator(
  SMTKMESHSESSION_EXPORT,
  smtk::bridge::mesh::ImportOperator,
  mesh_import,
  "import",
  ImportOperator_xml,
  smtk::bridge::mesh::Session);
