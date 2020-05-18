//=============================================================================
//
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//=============================================================================
#include "smtk/session/multiscale/Resource.h"

namespace smtk
{
namespace session
{
namespace multiscale
{

Resource::Resource(const smtk::common::UUID& id, smtk::resource::Manager::Ptr manager)
  : smtk::resource::DerivedFrom<Resource, smtk::session::mesh::Resource>(id, manager)
{
}

Resource::Resource(smtk::resource::Manager::Ptr manager)
  : smtk::resource::DerivedFrom<Resource, smtk::session::mesh::Resource>(manager)
{
}
} // namespace multiscale
} // namespace session
} // namespace smtk
