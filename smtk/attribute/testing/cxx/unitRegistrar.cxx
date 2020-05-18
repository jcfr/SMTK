//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "smtk/attribute/Registrar.h"
#include "smtk/attribute/Resource.h"

#include "smtk/common/Managers.h"
#include "smtk/common/testing/cxx/helpers.h"

#include "smtk/resource/Manager.h"
#include "smtk/resource/Registrar.h"

// Proves that smtk::attribute::Registrar properly registers and unregisters
// Evaluators to the EvaluatorFactory of managed Attribute Resources.
void testDefaultEvaluatorRegistration()
{
  auto managers = smtk::common::Managers::create();
  smtk::resource::Registrar::registerTo(managers);
  smtk::attribute::Registrar::registerTo(managers);

  auto resourceManager = managers->get<smtk::resource::Manager::Ptr>();
  auto evaluatorManager = managers->get<smtk::attribute::EvaluatorManager::Ptr>();

  smtk::attribute::Registrar::registerTo(evaluatorManager);

  smtk::attribute::Registrar::registerTo(resourceManager);
  auto attRes = resourceManager->create<smtk::attribute::Resource>();
  auto infixDefinition = attRes->createDefinition("infixExpression");

  smtkTest(
    attRes->evaluatorFactory().addDefinitionForEvaluator(
      "InfixExpressionEvaluator", infixDefinition->type()) == true,
    "Expected to be able to add definition for InfixExpressionEvaluator because "
    "Registrar already registered InfixExpressionEvaluator.")

    smtk::attribute::Registrar::unregisterFrom(evaluatorManager);

  auto fooDefinition = attRes->createDefinition("fooExpression");

  smtkTest(
    attRes->evaluatorFactory().addDefinitionForEvaluator(
      "InfixExpressionEvaluator", fooDefinition->type()) == false,
    "Expected failure to add definition to InfixExpressionEvaluator because "
    "Registrar unregistered InfixExpressionEvaluator.")
}

int unitRegistrar(int /*argc*/, char** const /*argc*/)
{
  testDefaultEvaluatorRegistration();

  return 0;
}
