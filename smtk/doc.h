//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#ifndef __smtk_doc_h
#define __smtk_doc_h

/*!\mainpage Simulation Modeling Tool Kit
 *
 * The Simulation Modeling Tool Kit (SMTK) is a library for preparing
 * one or more simulation runs from descriptions of their attributes.
 * Inputs may include
 *
 * + a description of the simulation package(s) to be used,
 * + a description of the simulation domain,
 * + attributes defined over the domain (boundary and initial conditions
 *   as well as global simulation and solver options), and optionally
 * + a solid model (from which an analysis mesh may be generated by a mesher)
 * + an analysis mesh.
 *
 * The library uses the ::smtk namespace and divides classes
 * into components for smtk::attribute definition and smtk::model definition.
 */

/**\brief The main namespace for the Simulation Modeling Tool Kit (SMTK).
  *
  */
namespace smtk
{

/**\brief Classes used throughout the toolkit.
    *
    */
namespace common
{
}

/**\brief Define attributes describing simulation inputs.
    *
    */
namespace attribute
{
}

/**\brief Represent geometric and topological models of simulation domains.
    *
    * The Manager class holds records defining one or more geometric-
    * and/or topological-domain decompositions;
    * it maps smtk::common::UUID values to Entity, Arrangement, and
    * Tessellation instances.
    * However, most developers will use the EntityRef classes
    * (Vertex, Edge, Face, Volume, VertexUse, EdgeUse, FaceUse, VolumeUse,
    * Chain, Loop, Shell, Group, Model, and Instance)
    * to access this information.
    * EntityRef is a base class for traversing records in Manager
    * and provides some capability for modifying the model.
    * Attributes may be defined on any record in storage by virtue of the
    * fact that all records in storage are named by their UUID.
    *
    * A set of classes exist for presenting model information to users.
    * DescriptivePhrase is an abstract base class with subclasses
    * EntityPhrase, EntityListPhrase, PropertyValuePhrase, and
    * PropertyListPhrase. These instances are placed into a hierarchy
    * that describe the model in a context; in a functional modeling
    * context, perhaps only descriptions of models and their groups will
    * be displayed. When assigning geometry to functional groups,
    * perhaps only geometric cells of a particular dimension will be shown.
    * The SubphraseGenerator class is what determines the particular
    * hierarchy, and a subclass will generally be written for each
    * context in which model Manager should be presented.
    *
    * If built with VTK, several classes beginning with "vtk" are available
    * for rendering and interacting with model entities which have
    * tessellation information.
    *
    * If built with Qt, the QEntityItemModel, QEntityItemDelegate, and
    * QEntityItemEditor classes may be used to display model information
    * as exposed by a hierarchy of DescriptivePhrase instances.
    */
namespace model
{
}

/**\brief Tools for exporting simulation input decks from attributes.
    *
    */
namespace simulation
{
}

/**\brief I/O utilities for the toolkit.
    *
    */
namespace io
{
}

/**\brief Sessions to solid modeling kernels.
    *
    */
namespace bridge
{
/**\brief A session using the Common Geometry Module (Argonne).
      *
      */
namespace cgm
{
}
/**\brief A session that imports Exodus meshes.
      *
      */
namespace exodus
{
}
/**\brief A forwarding session that uses Remus.
      *
      */
namespace remote
{
}
}

/**\brief Representations of SMTK components in user interfaces.
    *
    */
namespace view
{
}
}

#endif // __smtk_doc_h
