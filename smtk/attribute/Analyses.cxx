//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "smtk/attribute/Analyses.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/GroupItemDefinition.h"
#include "smtk/attribute/Resource.h"
#include "smtk/attribute/StringItemDefinition.h"
#include "smtk/attribute/VoidItemDefinition.h"

using namespace smtk::attribute;

namespace
{
// Helper function to build an Attribute Definition Item to represent an Analysis
template <typename DefinitionPtrType>
void buildAnalysisItemHelper(const Analyses::Analysis* analysis, DefinitionPtrType& def)
{
  // Childless Analyses are represented as a Void Item
  if (!analysis->children().size())
  {
    auto vitem = def->template addItemDefinition<VoidItemDefinition>(analysis->name());
    vitem->setIsOptional(true);
    vitem->setLabel(analysis->displayedName());
    return;
  }
  // Exlcusive Analyses are represented as a String Item with Discrete Values
  // One value for each of its child Analysis
  if (analysis->isExclusive())
  {
    auto sitem = def->template addItemDefinition<StringItemDefinition>(analysis->name());
    sitem->setIsOptional(true);
    sitem->setLabel(analysis->displayedName());
    for (auto child : analysis->children())
    {
      child->buildAnalysisItem(sitem);
    }
    return;
  }
  // Non Exclusive Analyses are represented as a Group Item
  auto gitem = def->template addItemDefinition<GroupItemDefinition>(analysis->name());
  gitem->setIsOptional(true);
  gitem->setLabel(analysis->displayedName());
  for (auto child : analysis->children())
  {
    child->buildAnalysisItem(gitem);
  }
};
}

/// \brief Method to set an Analysis' parent Analysis.
///
/// Initially an Analysis' parent is nullptr.  When setting an Analysis' parent, if the parent
/// is a pointer to the analysis itself, false is returned and nothing is changed.  If the
/// parent is the same as the analysis' current parent, nothing is changed and true is returned. If the
/// prior parent is not nullptr, the Analysis is removed from the original parent's children.
/// if the new parent is not nullptr then the Analysis is added to its children.
bool Analyses::Analysis::setParent(Analysis* p)
{
  // Is this a different parent than what we currently have?
  if (p == m_parent)
  {
    return true; // Nothing to do
  }

  // Are we trying to set the parent to ourself?
  if (p == this)
  {
    return false;
  }

  // If we have a parent set then we need to remove ourselves from its
  // children list
  if (m_parent != nullptr)
  {
    for (auto it = m_parent->m_children.begin(); it != m_parent->m_children.end(); ++it)
    {
      if (*it == this)
      {
        it = m_parent->m_children.erase(it);
        break; // found what we were looking for
      }
    }
  }
  // Ok if we have a new parent then add this to its list
  if (p != nullptr)
  {
    p->m_children.push_back(this);
  }
  m_parent = p;
  return true;
}

std::set<std::string> Analyses::Analysis::categories() const
{
  auto result = m_categories;
  for (auto p = m_parent; p != nullptr; p = p->m_parent)
  {
    result.insert(p->m_categories.begin(), p->m_categories.end());
  }
  return result;
}

/// @{
/// If the Item is being added either to a Attribute Definition or to a GroupItemDefinition then:
/// * if the Analysis has no children - an optional VoidItemDefinition is created
/// * else if its Exclusive Property is true - an optional StringItemDefinition is created
/// * else an optional GroupItemDefinition will be created
/// In any case the ItemDefinition created will be named using the Analysis' name and
/// if it has children, it will be passed to their buildAnalysisItem method
///
/// If the Analysis is being added to a StringItemDefinition, a discrete value using the
/// Analysis' name will be added to the definition.  If it does not have any children the method
/// simply returns.
/// Else if it's Exclusive Property is true, then in addition a new
/// StringItemDefinition will be created and added as a conditional item to definition passed
/// in, else an optional GroupItemDefinition is created and added as a conditional item.  In either
/// case the newly created ItemDefinition will be named using the Analysis' name and it will be passed
/// to it's children's buildAnalysisItem method.

void Analyses::Analysis::buildAnalysisItem(DefinitionPtr& def) const
{
  buildAnalysisItemHelper<DefinitionPtr>(this, def);
}

void Analyses::Analysis::buildAnalysisItem(GroupItemDefinitionPtr& pitem) const
{
  buildAnalysisItemHelper<GroupItemDefinitionPtr>(this, pitem);
}

void Analyses::Analysis::buildAnalysisItem(StringItemDefinitionPtr& pitem) const
{
  pitem->addDiscreteValue(m_name);
  if (!m_children.size())
  {
    return;
  }

  if (m_exclusive)
  {
    auto sitem = pitem->addItemDefinition<StringItemDefinition>(m_name);
    sitem->setLabel(this->displayedName());
    pitem->addConditionalItem(m_name, m_name);
    for (auto child : m_children)
    {
      child->buildAnalysisItem(sitem);
    }
    return;
  }

  auto gitem = pitem->addItemDefinition<GroupItemDefinition>(m_name);
  gitem->setLabel(this->displayedName());
  pitem->addConditionalItem(m_name, m_name);
  for (auto child : m_children)
  {
    child->buildAnalysisItem(gitem);
  }
}
///@}
Analyses::~Analyses()
{
  // Delete all analyses contained
  for (auto it = m_analyses.begin(); it != m_analyses.end(); ++it)
  {
    delete *it;
  }
  m_analyses.clear();
}

Analyses::Analysis* Analyses::create(const std::string& name)
{
  // Let first see if we already have one with that name if so
  // don't create anything and return nullptr
  if (this->find(name) != nullptr)
  {
    return nullptr;
  }

  m_analyses.push_back(new Analysis(name));
  return m_analyses.back();
}

Analyses::Analysis* Analyses::find(const std::string& name) const
{
  for (auto it = m_analyses.begin(); it != m_analyses.end(); ++it)
  {
    if ((*it)->name() == name)
    {
      return *it;
    }
  }
  return nullptr;
}
std::vector<Analyses::Analysis*> Analyses::topLevel() const
{
  std::vector<Analysis*> result;
  for (auto child : m_analyses)
  {
    if (child->parent() == nullptr)
    {
      result.push_back(child);
    }
  }
  return result;
}

bool Analyses::setAnalysisParent(const std::string& analysis, const std::string& parent)
{
  // First find both
  Analysis* theAnalysis = this->find(analysis);
  Analysis* theParent = this->find(parent);

  if ((theAnalysis == nullptr) || (theParent == nullptr))
  {
    return false;
  }

  return theAnalysis->setParent(theParent);
}

/// The Definition will be named using \p type and will be part of \p resource.  If \p type already exists
/// in \p resource then no Definition will be created and a nullptr will be returned.
/// If the top level exclusive property is false then each top level Analysis's buildAnalysisItem method
/// will be called using the newly created Definition.  Else a new StringItemDefinition will be created
/// using \p label as its name and each top level Analysis's buildAnalysisItem method will be called using
/// it as its input arguement.
///
/// The resulting definition will have the following structure:
/// * Either one item (in the case of Exclusive Toplevel Definitions) with a discrete value
/// per toplevel Analysis, or one item per toplevel Analysis
/// * Every Analysis with children will generate either a string or group item depnding the its Exclusive
/// Property
/// * Childless Analysis Instances will only generate an item if its parent has Exclusive = false or its
/// a toplevel Anaysis and Analyses has TopLevelExclusive = false.
/// When Exclusive is true (in ether the Analysis or Analyses case) the resulting String Item will have
/// a set of discrete values (one for either child or toplevel Analysis) and optionally an item representing
/// an Anlysis' children associated with that discrete value.
DefinitionPtr Analyses::buildAnalysesDefinition(
  ResourcePtr resource, const std::string& type, const std::string& label) const
{
  // First see if the defintion already exists
  auto def = resource->findDefinition(type);
  if (def)
  {
    return smtk::attribute::DefinitionPtr();
  }

  // Ok lets build a definition with the following rules:
  // 1. All items are optional and each respresents an Analysis
  // 2. An Analysis with no children is a void item
  // 3. An Analsyis with children is a group item

  def = resource->createDefinition(type);
  auto topAnalyses = this->topLevel();

  if (m_topLevelExclusive)
  {
    auto sitem = def->addItemDefinition<StringItemDefinition>(label);
    for (auto child : topAnalyses)
    {
      child->buildAnalysisItem(sitem);
    }
  }
  else
  {
    for (auto child : topAnalyses)
    {
      child->buildAnalysisItem(def);
    }
  }
  return def;
}
