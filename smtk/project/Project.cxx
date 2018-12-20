//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/project/Project.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/DirectoryItem.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/Resource.h"
#include "smtk/attribute/ResourceItem.h"
#include "smtk/attribute/StringItem.h"
#include "smtk/attribute/VoidItem.h"
#include "smtk/common/TypeName.h"
#include "smtk/io/AttributeReader.h"
#include "smtk/io/AttributeWriter.h"
#include "smtk/operation/Manager.h"
#include "smtk/operation/operators/ImportPythonOperation.h"
#include "smtk/operation/operators/ImportResource.h"
#include "smtk/operation/operators/ReadResource.h"
#include "smtk/operation/operators/WriteResource.h"
#include "smtk/project/json/jsonProjectDescriptor.h"
#include "smtk/resource/Manager.h"

#include "boost/filesystem.hpp"

#include <exception>
#include <fstream>

namespace
{
std::string PROJECT_FILENAME = ".smtkproject";
int SUCCEEDED = static_cast<int>(smtk::operation::Operation::Outcome::SUCCEEDED); // 3
}

namespace smtk
{
namespace project
{
Project::Project()
{
}

Project::~Project()
{
  this->close();
}

std::vector<smtk::resource::ResourcePtr> Project::getResources() const
{
  std::vector<smtk::resource::ResourcePtr> resourceList;

  for (auto& rd : this->m_resourceDescriptors)
  {
    auto resource = this->m_resourceManager->get(rd.m_uuid);
    resourceList.push_back(resource);
  }

  return resourceList;
}

void Project::setCoreManagers(
  smtk::resource::ManagerPtr resourceManager, smtk::operation::ManagerPtr operationManager)
{
  if (!this->m_resourceDescriptors.empty())
  {
    throw std::runtime_error("Cannot change core managers on open project");
  }
  this->m_resourceManager = resourceManager;
  this->m_resourceManager->registerResource<smtk::attribute::Resource>();
  this->m_operationManager = operationManager;
}

bool Project::build(smtk::attribute::AttributePtr specification, smtk::io::Logger& logger,
  bool replaceExistingDirectory)
{
  // Note that specification is the "new-project" definition in NewProject.sbt.
  // Validate starting conditions
  if (!specification->isValid())
  {
    smtkErrorMacro(logger, "invalid project specification");
    return false;
  }

  // (Future) check that at least 1 resource specified?

  this->m_name = specification->findString("project-name")->value(0);
  this->m_directory = specification->findDirectory("project-directory")->value(0);

  // Check if project directory already exists
  boost::filesystem::path boostDirectory(this->m_directory);
  if (boost::filesystem::exists(boostDirectory) && !boost::filesystem::is_empty(boostDirectory))
  {
    if (replaceExistingDirectory)
    {
      boost::filesystem::remove_all(boostDirectory);
    }
    else
    {
      smtkErrorMacro(
        logger, "Cannot create project in existing directory: \"" << this->m_directory << "\"");
      return false;
    }

  } // if (directory already exists)
  boost::filesystem::create_directory(boostDirectory);

  // Import the model resource
  ResourceDescriptor modelDescriptor;
  auto modelFileItem = specification->findFile("model-file");
  if (modelFileItem->isEnabled())
  {
    std::string modelPath = modelFileItem->value(0);
    bool copyNativeModel = specification->findVoid("copy-model-file")->isEnabled();
    if (!this->importModel(modelPath, copyNativeModel, modelDescriptor, logger))
    {
      return false;
    }
    this->m_resourceDescriptors.push_back(modelDescriptor);
  } // if (modelFileItem emabled)

  // Import the attribute template
  ResourceDescriptor attDescriptor;
  auto attFileItem = specification->findFile("simulation-template");
  if (attFileItem->isEnabled())
  {
    std::string attPath = attFileItem->value(0);
    bool success = this->importAttributeTemplate(attPath, attDescriptor, logger);
    if (!success)
    {
      return false;
    }
    this->m_resourceDescriptors.push_back(attDescriptor);
  } // if (attFileItem enabled)

  // Link attribute resource to model resource
  if (!modelDescriptor.m_uuid.isNull() && !attDescriptor.m_uuid.isNull())
  {
    auto attResource = this->m_resourceManager->get(attDescriptor.m_uuid);
    auto modelResource = this->m_resourceManager->get(modelDescriptor.m_uuid);
    smtk::dynamic_pointer_cast<smtk::attribute::Resource>(attResource)->associate(modelResource);
  }

  // Write project files and return
  return this->save(logger);
}

bool Project::save(smtk::io::Logger& logger) const
{
  boost::filesystem::path boostDirectory(this->m_directory);

  // For now, saving a project consists of saving its resources.
  // Later may include saving project metadata
  for (auto& rd : this->m_resourceDescriptors)
  {
    auto resource = this->m_resourceManager->get(rd.m_uuid);
    auto writer = this->m_operationManager->create<smtk::operation::WriteResource>();
    writer->parameters()->associate(resource);
    auto result = writer->operate();
    int outcome = result->findInt("outcome")->value(0);
    if (outcome != SUCCEEDED)
    {
      smtkErrorMacro(
        logger, "Error writing resource file " << resource->location() << ", outcome: " << outcome);
      return false;
    }
  } // for (rd)

  return this->writeProjectFile(logger);
} // save()

bool Project::close()
{
  this->releaseExportOperator();

  // Release resources
  for (auto& rd : this->m_resourceDescriptors)
  {
    auto resourcePtr = this->m_resourceManager->get(rd.m_uuid);
    if (resourcePtr)
    {
      this->m_resourceManager->remove(resourcePtr);
    }
  }
  this->m_resourceDescriptors.clear();
  this->m_name.clear();
  this->m_directory.clear();

  return true;
} // close()

bool Project::open(const std::string& location, smtk::io::Logger& logger)
{
  // Setup path to directory and .cmbproject file
  boost::filesystem::path directoryPath;
  boost::filesystem::path dotfilePath;

  boost::filesystem::path inputPath(location);
  if (!boost::filesystem::exists(inputPath))
  {
    smtkErrorMacro(logger, "Specified project path not found" << location);
    return false;
  }

  if (boost::filesystem::is_directory(inputPath))
  {
    directoryPath = inputPath;
    dotfilePath = directoryPath / boost::filesystem::path(PROJECT_FILENAME);
    if (!boost::filesystem::exists(dotfilePath))
    {
      smtkErrorMacro(logger, "No \"" << PROJECT_FILENAME << "\" file in specified directory");
      return false;
    }
  }
  else if (inputPath.filename().string() != PROJECT_FILENAME)
  {
    smtkErrorMacro(logger, "Invalid project filename, should be \"" << PROJECT_FILENAME << "\"");
    return false;
  }
  else
  {
    dotfilePath = inputPath;
    directoryPath = inputPath.parent_path();
  }

  // Load project file
  std::ifstream dotFile;
  dotFile.open(dotfilePath.string().c_str(), std::ios_base::in | std::ios_base::ate);
  if (!dotFile)
  {
    smtkErrorMacro(logger, "Failed loading \"" << PROJECT_FILENAME << "\" file");
    return false;
  }
  auto fileSize = dotFile.tellg();
  std::string dotFileContents;
  dotFileContents.reserve(fileSize);

  dotFile.seekg(0, std::ios_base::beg);
  dotFileContents.assign(
    (std::istreambuf_iterator<char>(dotFile)), std::istreambuf_iterator<char>());

  ProjectDescriptor descriptor;
  try
  {
    parse_json(dotFileContents, descriptor); // (static function in jsonProjectDescriptor.h)
  }
  catch (std::exception& ex)
  {
    smtkErrorMacro(logger, "Error parsing \"" << PROJECT_FILENAME << "\" file");
    smtkErrorMacro(logger, ex.what());
    return false;
  }

  this->m_name = descriptor.m_name;
  this->m_directory = descriptor.m_directory;
  this->m_resourceDescriptors = descriptor.m_resourceDescriptors;

  return this->loadResources(directoryPath.string(), logger);
} // open()

bool Project::importModel(const std::string& importPath, bool copyNativeModel,
  ResourceDescriptor& descriptor, smtk::io::Logger& logger)
{
  smtkDebugMacro(logger, "Loading model " << importPath);

  boost::filesystem::path boostImportPath(importPath);
  boost::filesystem::path boostDirectory(this->m_directory);

  // Copy the import (native) model file
  if (copyNativeModel)
  {
    auto copyPath = boostDirectory / boostImportPath.filename();
    boost::filesystem::copy_file(importPath, copyPath);
    descriptor.m_importLocation = copyPath.string();

    // And update the import path to use the copied file
    boostImportPath = copyPath;
  }

  // Create the import operator
  auto importOp = this->m_operationManager->create<smtk::operation::ImportResource>();
  if (!importOp)
  {
    smtkErrorMacro(logger, "Import operator not found");
    return false;
  }

  int outcome;

  // Run the import operator
  importOp->parameters()->findFile("filename")->setValue(importPath);
  auto importOpResult = importOp->operate();
  outcome = importOpResult->findInt("outcome")->value(0);
  if (outcome != SUCCEEDED)
  {
    smtkErrorMacro(logger, "Error importing file " << importPath);
    return false;
  }
  auto modelResource = importOpResult->findResource("resource")->value(0);

  // Set location to project directory
  auto modelFilename = boostImportPath.filename().string() + ".smtk";
  auto smtkPath = boostDirectory / boost::filesystem::path(modelFilename);
  modelResource->setLocation(smtkPath.string());

  // Update the descriptor
  descriptor.m_filename = modelFilename;
  descriptor.m_identifier = modelResource->name();
  descriptor.m_importLocation = importPath;
  descriptor.m_typeName = modelResource->typeName();
  descriptor.m_uuid = modelResource->id();

  return true; // success
} // importModel()

bool Project::importAttributeTemplate(
  const std::string& location, ResourceDescriptor& descriptor, smtk::io::Logger& logger)
{
  smtkDebugMacro(logger, "Loading templateFile: " << location);

  // Read from specified location
  auto attResource = smtk::attribute::Resource::create();
  smtk::io::AttributeReader reader;
  bool readErr = reader.read(attResource, location, true, logger);
  if (readErr)
  {
    return false; // invert from representing "error" to representing "success"
  }
  this->m_resourceManager->add(attResource);

  // Update descriptor
  if (descriptor.m_identifier == "")
  {
    descriptor.m_identifier = "default";
  }
  descriptor.m_filename = descriptor.m_identifier + ".sbi.smtk";
  descriptor.m_importLocation = location;
  descriptor.m_typeName = attResource->typeName();
  descriptor.m_uuid = attResource->id();

  boost::filesystem::path boostDirectory(this->m_directory);
  boost::filesystem::path resourcePath =
    boostDirectory / boost::filesystem::path(descriptor.m_filename);
  attResource->setLocation(resourcePath.string());
  return true;
} // importAttributeTemplate()

bool Project::writeProjectFile(smtk::io::Logger& logger) const
{
  // Init ProjectDescriptor structure
  ProjectDescriptor descriptor;
  descriptor.m_name = this->m_name;
  descriptor.m_directory = this->m_directory;
  descriptor.m_resourceDescriptors = this->m_resourceDescriptors;

  // Get json string
  std::string dotFileContents =
    dump_json(descriptor); // (static function in jsonProjectDescriptor.h)

  // Write file
  std::ofstream projectFile;
  auto path =
    boost::filesystem::path(this->m_directory) / boost::filesystem::path(PROJECT_FILENAME);
  projectFile.open(path.string().c_str(), std::ofstream::out | std::ofstream::trunc);
  if (!projectFile)
  {
    smtkErrorMacro(logger, "Unable to write to project file " << path.string());
    return false;
  }
  projectFile << dotFileContents << "\n";
  projectFile.close();

  return true;
}

bool Project::loadResources(const std::string& path, smtk::io::Logger& logger)
{
  // Part of opening project from disk
  boost::filesystem::path directoryPath(path);

  for (auto& descriptor : this->m_resourceDescriptors)
  {
    auto filePath = directoryPath / boost::filesystem::path(descriptor.m_filename);
    // auto inputPath = filePath.string();
    std::cout << "Loading " << filePath.string() << std::endl;

    // Create a read operator
    auto readOp = this->m_operationManager->create<smtk::operation::ReadResource>();
    if (!readOp)
    {
      smtkErrorMacro(logger, "Read Resource operator not found");
      return false;
    }
    readOp->parameters()->findFile("filename")->setValue(filePath.string());
    auto readOpResult = readOp->operate();

    // Check outcome
    int outcome = readOpResult->findInt("outcome")->value(0);
    if (outcome != SUCCEEDED)
    {
      smtkErrorMacro(logger, "Error loading resource from: " << filePath.string());
      return false;
    }
  } // for (descriptor)

  return true;
} // loadResources()

smtk::operation::OperationPtr Project::getExportOperator(smtk::io::Logger& logger, bool reset)
{
  // Check if already loaded
  if (!!m_exportOperator)
  {
    if (reset)
    {
      this->releaseExportOperator();
    }
    else
    {
      return m_exportOperator;
    }
  }

  // For now, find export operator based on fixed relative path from simulation template
  // to the sbt import directory.
  // Future: add .smtk info file to workflow directories

  // Find the simulation attribute resource.
  ResourceDescriptor simAttDescriptor;
  for (auto& descriptor : m_resourceDescriptors)
  {
    if (descriptor.m_typeName == smtk::common::typeName<smtk::attribute::Resource>())
    {
      simAttDescriptor = descriptor;
      break;
    }
  } // for

  if (simAttDescriptor.m_filename == "")
  {
    smtkErrorMacro(logger, "simulation attribute not found, so no export operator defined");
    return nullptr;
  }

  if (simAttDescriptor.m_importLocation == "")
  {
    smtkErrorMacro(
      logger, "simulation resource missing import location - cannot find export operator");
    return nullptr;
  }

  // Copy the import location and change extension from .sbt to .py
  std::string location(simAttDescriptor.m_importLocation);
  std::string key(".sbt");
  auto pos = location.rfind(key);
  if (pos == std::string::npos)
  {
    smtkErrorMacro(logger, "import location (" << location << ") did not end in .sbt");
    return nullptr;
  }

  location.replace(pos, key.length(), ".py");
  boost::filesystem::path locationPath(location);

  if (!boost::filesystem::exists(locationPath))
  {
    smtkErrorMacro(logger, "Could not find export operator file " << location);
    return nullptr;
  }

  std::cout << "Importing python operator: " << location << std::endl;

  auto importPythonOp = m_operationManager->create<smtk::operation::ImportPythonOperation>();
  if (!importPythonOp)
  {
    smtkErrorMacro(logger, "Could not create \"import python operation\"");
    return nullptr;
  }

  // Set the input python operation file name
  importPythonOp->parameters()->findFile("filename")->setValue(location);

  smtk::operation::Operation::Result result;
  try
  {
    // Execute the operation
    result = importPythonOp->operate();
  }
  catch (std::exception& e)
  {
    smtkErrorMacro(smtk::io::Logger::instance(), e.what());
    return nullptr;
  }

  // Test the results for success
  int outcome = result->findInt("outcome")->value();
  if (outcome != static_cast<int>(smtk::operation::Operation::Outcome::SUCCEEDED))
  {
    smtkErrorMacro(smtk::io::Logger::instance(),
      "\"import python operation\" operation failed, outcome " << outcome);
    return nullptr;
  }

  // On success, the ImportPythonOperation creates a "unique_name" value.
  // Use that string to create the export operator, and save that string to (later) release
  // the export operator.
  m_exportOperatorUniqueName = result->findString("unique_name")->value();
  m_exportOperator = m_operationManager->create(m_exportOperatorUniqueName);
  return m_exportOperator;
}

void Project::releaseExportOperator()
{
  if (!m_exportOperator || !m_operationManager)
  {
    return;
  }

  m_operationManager->unregisterOperation(m_exportOperatorUniqueName);
  m_exportOperator = nullptr;
  m_exportOperatorUniqueName = "";
}

} // namespace project
} // namespace smtk
