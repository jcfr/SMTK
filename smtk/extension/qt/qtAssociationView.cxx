//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "smtk/extension/qt/qtAssociationView.h"

#include "smtk/extension/qt/qtActiveObjects.h"
#include "smtk/extension/qt/qtAssociationWidget.h"
#include "smtk/extension/qt/qtAttribute.h"
#include "smtk/extension/qt/qtReferencesWidget.h"
#include "smtk/extension/qt/qtTableWidget.h"
#include "smtk/extension/qt/qtUIManager.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/Definition.h"

#include "smtk/view/View.h"
#include "ui_qtAssociationView.h"

#include <QMessageBox>
#include <QModelIndex>
#include <QModelIndexList>
#include <QPointer>
#include <QStyleOptionViewItem>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

#include <iostream>
#include <set>
using namespace smtk::attribute;
using namespace smtk::extension;

class qtAssociationViewInternals : public Ui::qtAssociationView
{
public:
  const QList<smtk::attribute::DefinitionPtr> getCurrentDefs(const QString strCategory) const
  {

    if (this->AttDefMap.keys().contains(strCategory))
    {
      return this->AttDefMap[strCategory];
    }
    return this->AllDefs;
  }

  QPointer<qtAssociationWidget> AssociationsWidget;

  // <category, AttDefinitions>
  QMap<QString, QList<smtk::attribute::DefinitionPtr> > AttDefMap;

  // All definitions list
  QList<smtk::attribute::DefinitionPtr> AllDefs;

  std::vector<smtk::attribute::DefinitionPtr> m_attDefinitions;
  smtk::model::BitFlags m_modelEntityMask;
  std::map<std::string, smtk::view::View::Component> m_attCompMap;
};

qtBaseView* qtAssociationView::createViewWidget(const ViewInfo& info)
{
  qtAssociationView* view = new qtAssociationView(info);
  view->buildUI();
  return view;
}

qtAssociationView::qtAssociationView(const ViewInfo& info)
  : qtBaseView(info)
{
  this->Internals = new qtAssociationViewInternals;
}

qtAssociationView::~qtAssociationView()
{
  delete this->Internals;
}

const QMap<QString, QList<smtk::attribute::DefinitionPtr> >& qtAssociationView::attDefinitionMap()
  const
{
  return this->Internals->AttDefMap;
}

void qtAssociationView::createWidget()
{
  // this->Internals->AttDefMap has to be initialized before getAllDefinitions()
  // since the getAllDefinitions() call needs the categories list in AttDefMap
  // Create a map for all catagories so we can cluster the definitions
  this->Internals->AttDefMap.clear();
  const ResourcePtr attResource = this->uiManager()->attResource();
  std::set<std::string>::const_iterator it;
  const std::set<std::string>& cats = attResource->categories();

  for (it = cats.begin(); it != cats.end(); it++)
  {
    QList<smtk::attribute::DefinitionPtr> attdeflist;
    this->Internals->AttDefMap[it->c_str()] = attdeflist;
  }

  // Initialize definition info
  this->getAllDefinitions();

  this->Widget = new QFrame(this->parentWidget());
  this->Internals->setupUi(this->Widget);

  // the association widget
  this->Internals->AssociationsWidget =
    new qtAssociationWidget(this->Internals->associations, this);
  this->Internals->mainLayout->addWidget(this->Internals->AssociationsWidget);
  // signals/slots
  QObject::connect(this->Internals->AssociationsWidget, SIGNAL(attAssociationChanged()), this,
    SIGNAL(attAssociationChanged()));

  QObject::connect(this->Internals->attributes, SIGNAL(currentIndexChanged(int)), this,
    SLOT(onAttributeChanged(int)));

  this->updateModelAssociation();
}

void qtAssociationView::updateModelAssociation()
{
  this->onShowCategory();
}

smtk::attribute::AttributePtr qtAssociationView::getAttributeFromIndex(int index)
{
  Attribute* rawPtr = (index >= 0)
    ? static_cast<Attribute*>(
        this->Internals->attributes->itemData(index, Qt::UserRole).value<void*>())
    : nullptr;
  return rawPtr ? rawPtr->shared_from_this() : smtk::attribute::AttributePtr();
}

void qtAssociationView::onAttributeChanged(int index)
{
  auto att = this->getAttributeFromIndex(index);
  this->Internals->AssociationsWidget->showEntityAssociation(att);
}

void qtAssociationView::displayAttributes()
{
  this->Internals->attributes->blockSignals(true);
  this->Internals->attributes->clear();
  if (!this->Internals->m_attDefinitions.size())
  {
    this->Internals->attributes->blockSignals(false);
    return;
  }

  QList<smtk::attribute::DefinitionPtr> currentDefs =
    this->Internals->getCurrentDefs(this->uiManager()->currentCategory().c_str());
  std::set<AttributePtr, Attribute::CompareByName> atts;
  // Get all of the attributes that match the list of definitions
  foreach (attribute::DefinitionPtr attDef, currentDefs)
  {
    ResourcePtr attResource = attDef->resource();
    std::vector<smtk::attribute::AttributePtr> result;
    attResource->findAttributes(attDef, result);
    if (result.size())
    {
      atts.insert(result.begin(), result.end());
    }
  }
  for (auto att : atts)
  {
    QVariant vdata;
    vdata.setValue(static_cast<void*>(att.get()));
    this->Internals->attributes->addItem(att->name().c_str(), vdata);
  }
  this->Internals->attributes->blockSignals(false);
  if (atts.size())
  {
    this->Internals->attributes->setCurrentIndex(0);
    this->onAttributeChanged(0);
  }
}

void qtAssociationView::onShowCategory()
{
  this->displayAttributes();
}

void qtAssociationView::getAllDefinitions()
{
  smtk::view::ViewPtr view = this->getObject();
  if (!view)
  {
    return;
  }

  smtk::attribute::ResourcePtr resource = this->uiManager()->attResource();

  std::string attName, defName, val;
  smtk::attribute::AttributePtr att;
  smtk::attribute::DefinitionPtr attDef;
  bool flag;

  // The view should have a single internal component called InstancedAttributes
  if ((view->details().numberOfChildren() != 1) ||
    (view->details().child(0).name() != "AttributeTypes"))
  {
    // Should present error message
    return;
  }

  if (view->details().attribute("ModelEntityFilter", val))
  {
    smtk::model::BitFlags flags = smtk::model::Entity::specifierStringToFlag(val);
    this->Internals->m_modelEntityMask = flags;
  }
  else
  {
    this->Internals->m_modelEntityMask = 0;
  }

  std::vector<smtk::attribute::AttributePtr> atts;
  smtk::view::View::Component& attsComp = view->details().child(0);
  std::size_t i, n = attsComp.numberOfChildren();
  for (i = 0; i < n; i++)
  {
    if (attsComp.child(i).name() != "Att")
    {
      continue;
    }
    if (!attsComp.child(i).attribute("Type", defName))
    {
      continue;
    }

    attDef = resource->findDefinition(defName);
    if (attDef == nullptr)
    {
      continue;
    }

    this->Internals->m_attCompMap[defName] = attsComp.child(i);
    this->qtBaseView::getDefinitions(attDef, this->Internals->AllDefs);
    this->Internals->m_attDefinitions.push_back(attDef);
  }

  // sort the list
  std::sort(std::begin(this->Internals->AllDefs), std::end(this->Internals->AllDefs),
    [](smtk::attribute::DefinitionPtr a, smtk::attribute::DefinitionPtr b) {
      return a->displayedTypeName() < b->displayedTypeName();
    });

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
  foreach (smtk::attribute::DefinitionPtr adef, this->Internals->AllDefs)
  {
    foreach (QString category, this->Internals->AttDefMap.keys())
    {
      if (adef->isMemberOf(category.toStdString()) &&
        !this->Internals->AttDefMap[category].contains(adef))
      {
        this->Internals->AttDefMap[category].push_back(adef);
      }
    }
  }
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
}

bool qtAssociationView::isEmpty() const
{
  return (this->Internals->attributes->count() == 0);
}
