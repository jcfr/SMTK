//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/ComponentItem.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/ReferenceItem.h"
#include "smtk/attribute/ReferenceItemDefinition.h"
#include "smtk/attribute/Registrar.h"
#include "smtk/attribute/Resource.h"
#include "smtk/attribute/operators/Signal.h"
#include "smtk/common/Managers.h"
#include "smtk/model/Registrar.h"
#include "smtk/model/Resource.h"
#include "smtk/model/Volume.h"
#include "smtk/operation/Manager.h"
#include "smtk/operation/Registrar.h"
#include "smtk/plugin/Registry.h"
#include "smtk/resource/Manager.h"
#include "smtk/resource/Registrar.h"
#include "smtk/task/FillOutAttributes.h"
#include "smtk/task/GatherResources.h"
#include "smtk/task/Group.h"
#include "smtk/task/Instances.h"
#include "smtk/task/Manager.h"
#include "smtk/task/Registrar.h"
#include "smtk/task/Task.h"

#include "smtk/task/json/jsonManager.h"
#include "smtk/task/json/jsonTask.h"

#include "smtk/common/testing/cxx/helpers.h"

namespace
{

std::string configString = R"(
{
  "tasks": [
    {
      "id": 1,
      "type": "smtk::task::GatherResources",
      "title": "Load a model and attribute",
      "state": "completed",
      "auto-configure": true,
      "resources": [
        {
          "role": "model geometry",
          "type": "smtk::model::Resource",
          "max": 2
        },
        {
          "role": "simulation attribute",
          "type": "smtk::attribute::Resource"
        }
      ]
    },
    {
      "id": 2,
      "type": "smtk::task::Group",
      "mode": "parallel",
      "title": "Prepare simulation",
      "dependencies": [ 1 ],
      "children": {
        "tasks": [
          {
            "id": 2,
            "type": "smtk::task::FillOutAttributes",
            "title": "Mark up model",
            "attribute-sets": [
              {
                "role": "simulation attribute",
                "definitions": [
                  "Material",
                  "BoundaryCondition"
                ]
              }
            ]
          },
          {
            "id": 3,
            "type": "smtk::task::FillOutAttributes",
            "title": "Set simulation parameters",
            "attribute-sets": [
              {
                "role": "simulation attribute",
                "definitions": [
                  "SimulationParameters"
                ]
              }
            ]
          }
        ],
        "adaptors": [
          {
            "id": 1,
            "type": "smtk::task::adaptor::ResourceAndRole",
            "from": 1,
            "to": 2
          },
          {
            "id": 2,
            "type": "smtk::task::adaptor::ResourceAndRole",
            "from": 1,
            "to": 3
          }
        ]
      }
    }
  ],
  "//": [
    "For each dependency, we can store configuration information",
    "for the object used to adapt one task's output into",
    "configuration information for its dependent task. If no",
    "configuration is present for a dependency, we use the",
    "'Ignore' adaptor as a default – which never modifies the",
    "depedent task's configuration."
  ],
  "adaptors": [
    {
      "id": 1,
      "type": "smtk::task::adaptor::ResourceAndRole",
      "from": 1,
      "to": 2
    }
  ]
}
    )";
void printTaskStates(smtk::task::Manager::Ptr taskManager, const std::string& message)
{
  std::cout << "### " << message << " ###\n";
  int depth = 0;
  smtk::task::Task* parent = nullptr;
  std::function<smtk::common::Visit(smtk::task::Task&)> childVisitor;
  childVisitor = [&parent, &depth, &childVisitor](smtk::task::Task& task) {
    if (&task == parent)
    {
      return smtk::common::Visit::Continue; // Skip self when visiting children
    }
    std::string indent(depth * 2, ' ');
    std::cout << indent << &task << " : children? " << (task.hasChildren() ? "Y" : "N")
              << " parent " << task.parent() << " state " << smtk::task::stateName(task.state())
              << " " << task.title() << "\n";
    if (task.hasChildren())
    {
      parent = &task;
      ++depth;
      task.visit(smtk::task::Task::RelatedTasks::Child, childVisitor);
      --depth;
      parent = nullptr;
    }
    return smtk::common::Visit::Continue;
  };
  taskManager->taskInstances().visit(
    [&childVisitor](const std::shared_ptr<smtk::task::Task>& task) {
      if (!task->parent())
      {
        childVisitor(*task);
      }
      return smtk::common::Visit::Continue;
    });
}

} // anonymous namespace

int TestTaskGroup(int, char*[])
{
  using smtk::task::State;
  using smtk::task::Task;

  // Create managers
  auto managers = smtk::common::Managers::create();
  auto attributeRegistry = smtk::plugin::addToManagers<smtk::attribute::Registrar>(managers);
  auto resourceRegistry = smtk::plugin::addToManagers<smtk::resource::Registrar>(managers);
  auto operationRegistry = smtk::plugin::addToManagers<smtk::operation::Registrar>(managers);
  auto taskRegistry = smtk::plugin::addToManagers<smtk::task::Registrar>(managers);

  auto resourceManager = managers->get<smtk::resource::Manager::Ptr>();
  auto operationManager = managers->get<smtk::operation::Manager::Ptr>();
  auto taskManager = managers->get<smtk::task::Manager::Ptr>();

  auto attributeResourceRegistry =
    smtk::plugin::addToManagers<smtk::attribute::Registrar>(resourceManager);
  auto attributeOperationRegistry =
    smtk::plugin::addToManagers<smtk::attribute::Registrar>(operationManager);
  auto modelRegistry = smtk::plugin::addToManagers<smtk::model::Registrar>(resourceManager);
  auto taskTaskRegistry = smtk::plugin::addToManagers<smtk::task::Registrar>(taskManager);

  auto attrib = resourceManager->create<smtk::attribute::Resource>();
  auto model = resourceManager->create<smtk::model::Resource>();
  attrib->setName("simulation");
  model->setName("geometry");
  attrib->properties().get<std::string>()["project_role"] = "simulation attribute";
  model->properties().get<std::string>()["project_role"] = "model geometry";
  auto attribUUIDStr = attrib->id().toString();
  auto modelUUIDStr = model->id().toString();

  auto config = nlohmann::json::parse(configString);
  std::cout << config.dump(2) << "\n";
  bool ok = smtk::task::json::jsonManager::deserialize(managers, config);
  test(ok, "Failed to parse configuration.");
  test(taskManager->taskInstances().size() == 4, "Expected to deserialize 4 tasks.");

  smtk::task::GatherResources::Ptr gatherResources;
  smtk::task::FillOutAttributes::Ptr fillOutAttributes;
  taskManager->taskInstances().visit(
    [&gatherResources, &fillOutAttributes](const smtk::task::Task::Ptr& task) {
      if (!gatherResources)
      {
        gatherResources = std::dynamic_pointer_cast<smtk::task::GatherResources>(task);
      }
      if (!fillOutAttributes)
      {
        fillOutAttributes = std::dynamic_pointer_cast<smtk::task::FillOutAttributes>(task);
      }
      return smtk::common::Visit::Continue;
    });

  // Add components and signal to test FillOutAttributes.
  // First, the FillOutAttributes task is irrelevant
  printTaskStates(taskManager, "Incomplete resource gathering.");
  gatherResources->markCompleted(true);

  printTaskStates(taskManager, "Completed resource gathering.");

  auto volume1 = model->addVolume();
  auto volume2 = model->addVolume();
  auto def = attrib->createDefinition("Material");
  auto assoc = def->createLocalAssociationRule();
  assoc->setAcceptsEntries("smtk::model::Resource", "volume", true);
  assoc->setNumberOfRequiredValues(1);
  assoc->setIsExtensible(true);
  auto material1 = attrib->createAttribute("CarbonFiber", "Material");

  // Signal the change
  auto signal = operationManager->create<smtk::attribute::Signal>();
  signal->parameters()->findComponent("created")->appendValue(material1);
  auto result = signal->operate();
  // Now, the FillOutAttributes task is incomplete
  printTaskStates(taskManager, "Added a material attribute.");

  material1->associate(volume1.component());
  signal->parameters()->findComponent("created")->setNumberOfValues(0);
  signal->parameters()->findComponent("modified")->appendValue(material1);
  result = signal->operate();
  // Finally, the FillOutAttributes task is completable
  printTaskStates(taskManager, "Associated material to model.");

  std::string configString2;
  {
    // Round trip it.
    nlohmann::json config2;
    ok = smtk::task::json::jsonManager::serialize(managers, config2);
    test(ok, "Failed to serialize task manager.");
    configString2 = config2.dump(2);
    std::cout << configString2 << "\n";
  }
  {
    // Round trip a second time so we should be guaranteed to have
    // two machine-(not-hand-)generated strings to compare.
    auto managers2 = smtk::common::Managers::create();
    managers2->insert(resourceManager);
    managers2->insert(operationManager);
    auto taskManager2 = smtk::task::Manager::create();
    smtk::task::Registrar::registerTo(taskManager2);
    managers2->insert(taskManager2);
    auto config3 = nlohmann::json::parse(configString2);
    ok = smtk::task::json::jsonManager::deserialize(managers2, config3);
    test(ok, "Failed to parse second configuration.");
    test(taskManager2->taskInstances().size() == 4, "Expected to deserialize 4 tasks.");
    nlohmann::json config4;
    ok = smtk::task::json::jsonManager::serialize(managers2, config4);
    test(ok, "Failed to serialize second manager.");
    std::string configString3 = config4.dump(2);
    std::cout << "----\n" << configString3 << "\n";
    // TODO: The valid/invalid attribute UUIDs in FillOutAttributes's serialization
    // are not being preserved.
    // test(configString2 == configString3, "Failed to match strings.");
  }

  return 0;
}
