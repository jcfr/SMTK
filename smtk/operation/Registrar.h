//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#ifndef smtk_operation_Registrar_h
#define smtk_operation_Registrar_h

#include "smtk/CoreExports.h"

#include "smtk/attribute/Registrar.h"
#include "smtk/common/Managers.h"
#include "smtk/operation/Manager.h"
#include "smtk/resource/Registrar.h"

namespace smtk
{
namespace operation
{
class SMTKCORE_EXPORT Registrar
{
public:
  using Dependencies = std::tuple<attribute::Registrar, resource::Registrar>;

  static void registerTo(const smtk::common::Managers::Ptr&);
  static void unregisterFrom(const smtk::common::Managers::Ptr&);

  static void registerTo(const smtk::operation::Manager::Ptr&);
  static void unregisterFrom(const smtk::operation::Manager::Ptr&);
};
} // namespace operation
} // namespace smtk

#endif
