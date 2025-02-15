//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#ifndef smtk_session_multiscale_Registrar_h
#define smtk_session_multiscale_Registrar_h

#include "smtk/attribute/Registrar.h"
#include "smtk/mesh/resource/Registrar.h"
#include "smtk/model/Registrar.h"
#include "smtk/operation/Manager.h"
#include "smtk/operation/Registrar.h"
#include "smtk/resource/Manager.h"
#include "smtk/session/mesh/Registrar.h"
#include "smtk/session/multiscale/Exports.h"

namespace smtk
{
namespace session
{
namespace multiscale
{

class SMTKMULTISCALESESSION_EXPORT Registrar
{
public:
  using Dependencies =
    std::tuple<operation::Registrar, model::Registrar, attribute::Registrar, mesh::Registrar>;

  static void registerTo(const smtk::operation::Manager::Ptr&);
  static void unregisterFrom(const smtk::operation::Manager::Ptr&);

  static void registerTo(const smtk::resource::Manager::Ptr&);
  static void unregisterFrom(const smtk::resource::Manager::Ptr&);
};
} // namespace multiscale
} // namespace session
} // namespace smtk

#endif
