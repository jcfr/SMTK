//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================


#include "smtk/extension/qt/qtUIManager.h"

#include "smtk/extension/qt/qtItem.h"
#include "smtk/extension/qt/qtFileItem.h"
#include "smtk/extension/qt/qtMeshEntityItem.h"
#include "smtk/extension/qt/qtModelEntityItem.h"
#include "smtk/extension/qt/qtGroupView.h"
#include "smtk/extension/qt/qtRootView.h"
#include "smtk/extension/qt/qtInputsItem.h"
#include "smtk/extension/qt/qtAttributeView.h"
#include "smtk/extension/qt/qtInstancedView.h"
#include "smtk/extension/qt/qtSimpleExpressionView.h"
#include "smtk/extension/qt/qtDiscreteValueEditor.h"

#include <QTableWidget>
#include <QLayout>
#include <QMimeData>
#include <QClipboard>
#include <QSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QStringList>
#include <QIntValidator>
#include <QFrame>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QFontMetrics>

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/Definition.h"
#include "smtk/attribute/System.h"
#include "smtk/attribute/RefItem.h"
#include "smtk/attribute/RefItemDefinition.h"
#include "smtk/attribute/GroupItem.h"
#include "smtk/attribute/GroupItemDefinition.h"
#include "smtk/attribute/ValueItem.h"
#include "smtk/attribute/ValueItemDefinition.h"
#include "smtk/attribute/DoubleItem.h"
#include "smtk/attribute/DoubleItemDefinition.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/IntItemDefinition.h"
#include "smtk/attribute/StringItem.h"
#include "smtk/attribute/StringItemDefinition.h"

#include "smtk/view/Root.h"
#include "smtk/view/Attribute.h"
#include "smtk/view/Instanced.h"
#include "smtk/view/ModelEntity.h"
#include "smtk/view/SimpleExpression.h"

#include <math.h>

#if defined(_WIN32) //VS2008 is not c99 complient.
#include <float.h>
double nextafter(double x, double y)
{
  return _nextafter(x,y);
}
#endif

using namespace smtk::attribute;

//-----------------------------------------------------------------------------
qtDoubleValidator::qtDoubleValidator(QObject * inParent)
  :QDoubleValidator(inParent)
{
  this->UIManager = NULL;
}
//-----------------------------------------------------------------------------
void qtDoubleValidator::setUIManager(smtk::attribute::qtUIManager* uiman)
{
  this->UIManager = uiman;
}
//-----------------------------------------------------------------------------
void qtDoubleValidator::fixup(QString &input) const
{
  QLineEdit* editBox =
    static_cast<QLineEdit*>(this->property("MyWidget").value<void *>());
  if(!editBox)
    {
    return;
    }
  ValueItem* item =static_cast<ValueItem*>(
    editBox->property("AttItemObj").value<void *>());
  if(!item)
    {
    return;
    }
  int elementIdx = editBox->property("ElementIndex").toInt();
  if ( input.length() == 0 )
    {
    item->unset(elementIdx);
    this->UIManager->setWidgetColor(editBox, Qt::white);
    return;
    }

  const DoubleItemDefinition* dDef =
    dynamic_cast<const DoubleItemDefinition*>(item->definition().get());
  if(item->isSet(elementIdx))
    {
    input = item->valueAsString(elementIdx).c_str();
    }
  else if(dDef->hasDefault())
    {
    input = QString::number(dDef->defaultValue(elementIdx));
    }
  else
    {
    this->UIManager->setWidgetColor(editBox, this->UIManager->invalidValueColor());
    }
}

//-----------------------------------------------------------------------------
qtIntValidator::qtIntValidator(QObject * inParent)
:QIntValidator(inParent)
{
  this->UIManager = NULL;
}

//-----------------------------------------------------------------------------
void qtIntValidator::setUIManager(smtk::attribute::qtUIManager* uiman)
{
  this->UIManager = uiman;
}

//-----------------------------------------------------------------------------
void qtIntValidator::fixup(QString &input) const
{
  QLineEdit* editBox =
    static_cast<QLineEdit*>(this->property("MyWidget").value<void *>());
  if(!editBox)
    {
    return;
    }
  ValueItem* item =static_cast<ValueItem*>(
    editBox->property("AttItemObj").value<void *>());
  if(!item)
    {
    return;
    }

  int elementIdx = editBox->property("ElementIndex").toInt();
  if ( input.length() == 0 )
    {
    item->unset(elementIdx);
    this->UIManager->setWidgetColor(editBox, Qt::white);
    return;
    }

  const IntItemDefinition* iDef =
    dynamic_cast<const IntItemDefinition*>(item->definition().get());
  if(item->isSet(elementIdx))
    {
    input = item->valueAsString(elementIdx).c_str();
    }
  else if(iDef->hasDefault())
    {
    input = QString::number(iDef->defaultValue(elementIdx));
    }
  else
    {
    this->UIManager->setWidgetColor(editBox, this->UIManager->invalidValueColor());
    }
}

//-----------------------------------------------------------------------------
qtTextEdit::qtTextEdit(QWidget * inParent)
:QTextEdit(inParent)
{
}

//-----------------------------------------------------------------------------
QSize qtTextEdit::sizeHint() const
{
  return QSize(200, 70);
}

//----------------------------------------------------------------------------
qtUIManager::qtUIManager(smtk::attribute::System &system) :
  m_AttSystem(system), m_useInternalFileBrowser(false)
{
  this->RootView = NULL;

  if(system.rootView())
    {
    this->advFont.setBold(system.rootView()->advancedBold());
    this->advFont.setItalic(system.rootView()->advancedItalic());
    const double* rgba = system.rootView()->defaultColor();
    this->DefaultValueColor.setRgbF(rgba[0], rgba[1], rgba[2]);
    rgba = system.rootView()->invalidColor();
    this->InvalidValueColor.setRgbF(rgba[0], rgba[1], rgba[2]);
    }
  else
    { // default settings
    this->advFont.setBold(true);
    this->advFont.setItalic(false);
    this->DefaultValueColor.setRgbF(1.0, 1.0, 0.5);
    this->InvalidValueColor.setRgbF(1.0, 0.5, 0.5);
    }
  this->m_currentAdvLevel = 0;
}

//----------------------------------------------------------------------------
qtUIManager::~qtUIManager()
{
  if(this->RootView)
    {
    delete this->RootView;
    }
}

//----------------------------------------------------------------------------
void qtUIManager::initializeUI(QWidget* pWidget, bool useInternalFileBrowser)
{
  m_useInternalFileBrowser = useInternalFileBrowser;
  if(!this->m_AttSystem.rootView())
    {
    return;
    }
  if(this->RootView)
    {
    delete this->RootView;
    }
  this->internalInitialize();

  this->RootView = new qtRootView(
    this->m_AttSystem.rootView(), pWidget, this);
  this->RootView->showAdvanceLevel(this->m_currentAdvLevel);
}

//----------------------------------------------------------------------------
void qtUIManager::internalInitialize()
{
  smtk::view::RootPtr rs = this->m_AttSystem.rootView();
  const double *dcolor = rs->defaultColor();
  this->DefaultValueColor.setRgbF(dcolor[0], dcolor[1], dcolor[2], dcolor[3]);
  dcolor = rs->invalidColor();
  this->InvalidValueColor.setRgbF(dcolor[0], dcolor[1], dcolor[2], dcolor[3]);
  this->findDefinitionsLongLabels();

  // initialize initial advance level
  const std::map<int, std::string> &levels = this->m_AttSystem.advanceLevels();
  if(levels.size() > 0)
    {
    // use the minimum enum value as initial advance level
    std::map<int, std::string>::const_iterator ait = levels.begin();
    int minLevel = ait->first;
    ait++;
    for (; ait != levels.end(); ++ait)
      {
      minLevel = std::min(minLevel, ait->first);
      }
    // this->m_currentAdvLevel can not be lower than the minLevel
    this->m_currentAdvLevel = std::max(minLevel, this->m_currentAdvLevel);
    }
}

//----------------------------------------------------------------------------
void qtUIManager::setAdvanceLevel(int b)
{
  if(this->m_currentAdvLevel == b)
    {
    return;
    }

  this->m_currentAdvLevel = b;
  if(this->RootView)
    {
    this->RootView->showAdvanceLevel(b);
    }
}
//----------------------------------------------------------------------------
void qtUIManager::initAdvanceLevels(QComboBox* combo)
{
  const std::map<int, std::string> &levels = this->m_AttSystem.advanceLevels();
  if(levels.size() == 0)
    {
    // for backward compatibility, we automatically add
    // two levels which is implicitly supported in previous version
    combo->addItem("General", 0);
    combo->addItem("Advanced", 1);
    }
  else
    {
    std::map<int, std::string>::const_iterator ait;
    for (ait = levels.begin(); ait != levels.end(); ++ait)
      {
      combo->addItem(ait->second.c_str(), ait->first);
      }
    }
}

//----------------------------------------------------------------------------
// Generates widget for a single input view
// bypassing the RootView tab widget
qtBaseView* qtUIManager::initializeView(QWidget* pWidget,
                                 smtk::view::BasePtr smtkView,
                                 bool useInternalFileBrowser)
{
  m_useInternalFileBrowser = useInternalFileBrowser;
  if(!this->m_AttSystem.rootView())
    {
    return NULL;
    }
  if(this->RootView)
    {
    delete this->RootView;
    }
  this->internalInitialize();

  return this->createView(smtkView, pWidget);
}

//----------------------------------------------------------------------------
void qtUIManager::updateModelViews()
{
  if(!this->RootView)
    {
    return;
    }
  this->updateModelViews(this->RootView->getRootGroup());
}

//----------------------------------------------------------------------------
void qtUIManager::updateModelViews(qtGroupView* groupView)
{
  if(!groupView)
    {
    return;
    }
  foreach(qtBaseView* childView, groupView->childViews())
    {
    if(childView->getObject()->type() == smtk::view::Base::GROUP)
      {
      this->updateModelViews(qobject_cast<qtGroupView*>(childView));
      }
    else if(childView->getObject()->type() == smtk::view::Base::ATTRIBUTE ||
       childView->getObject()->type() == smtk::view::Base::MODEL_ENTITY)
      {
      childView->updateModelAssociation();
      }
    }
}

//----------------------------------------------------------------------------
std::string qtUIManager::currentCategory()
{
  return this->RootView ? this->RootView->currentCategory() : "";
}
//----------------------------------------------------------------------------
bool qtUIManager::categoryEnabled()
{
  return this->RootView ? this->RootView->categoryEnabled() : false;
}
//----------------------------------------------------------------------------
bool qtUIManager::passAdvancedCheck(int level)
{
  return (level <= this->advanceLevel());
}
//----------------------------------------------------------------------------
bool qtUIManager::passAttributeCategoryCheck(
  smtk::attribute::ConstDefinitionPtr AttDef)
{
  return this->passCategoryCheck(AttDef->categories());
}
//----------------------------------------------------------------------------
bool qtUIManager::passItemCategoryCheck(
  smtk::attribute::ConstItemDefinitionPtr ItemDef)
{
  return this->passCategoryCheck(ItemDef->categories());
}

//----------------------------------------------------------------------------
bool qtUIManager::passCategoryCheck(const std::set<std::string> & categories)
{
  return !this->categoryEnabled() ||
    categories.find(this->currentCategory()) != categories.end();
}
//----------------------------------------------------------------------------
void qtUIManager::processAttributeView(qtAttributeView* qtView)
{
  smtk::view::AttributePtr v = smtk::dynamic_pointer_cast<smtk::view::Attribute>(
    qtView->getObject());

  this->processBasicView(qtView);
}
//----------------------------------------------------------------------------
void qtUIManager::processInstancedView(qtInstancedView* qtView)
{
  smtk::view::InstancedPtr v = smtk::dynamic_pointer_cast<smtk::view::Instanced>(
    qtView->getObject());

  this->processBasicView(qtView);
}
////----------------------------------------------------------------------------
//void qtUIManager::processModelView(qtModelView* qtView)
//{
//  smtk::view::ModelPtr v = smtk::dynamic_pointer_cast<smtk::view::Model>(
//    qtView->getObject());

//  this->processBasicView(qtView);
//}
//----------------------------------------------------------------------------
void qtUIManager::processSimpleExpressionView(qtSimpleExpressionView* qtView)
{
  smtk::view::SimpleExpressionPtr v = smtk::dynamic_pointer_cast<smtk::view::SimpleExpression>(
    qtView->getObject());

  this->processBasicView(qtView);
}
//----------------------------------------------------------------------------
void qtUIManager::processGroupView(qtGroupView* pQtGroup)
{
  smtk::view::GroupPtr group = smtk::dynamic_pointer_cast<smtk::view::Group>(
    pQtGroup->getObject());
  this->processBasicView( pQtGroup);
  std::size_t i, n = group->numberOfSubViews();
  smtk::view::BasePtr v;
  qtBaseView* qtView = NULL;
  for (i = 0; i < n; i++)
    {
    v = group->subView(static_cast<int>(i));
    qtView = qtUIManager::createView(v, pQtGroup->widget());
    if(qtView)
      {
      pQtGroup->addChildView(qtView);
      }
    }
}

//----------------------------------------------------------------------------
void qtUIManager::processBasicView(qtBaseView* /*v*/)
{
  //node.append_attribute("Title").set_value(v->title().c_str());
  //if (v->iconName() != "")
  //  {
  //  node.append_attribute("Icon").set_value(v->title().c_str());
  //  }
}

//----------------------------------------------------------------------------
QString qtUIManager::clipBoardText()
{
  const QMimeData* const clipboard = QApplication::clipboard()->mimeData();
  return clipboard->text();
}

//----------------------------------------------------------------------------
void qtUIManager::setClipBoardText(QString& text)
{
  QApplication::clipboard()->setText(text);
}

//----------------------------------------------------------------------------
void qtUIManager::clearRoot()
{
  if(this->RootView)
    {
    delete this->RootView;
    this->RootView = NULL;
    }
}

//----------------------------------------------------------------------------
void qtUIManager::setInvalidValueColor(const QColor &color)
{
  this->InvalidValueColor = color;
}

//----------------------------------------------------------------------------
void qtUIManager::setWidgetColor(QWidget *widget, const QColor &color)
{
  QPalette pal = widget->palette();
  pal.setColor(QPalette::Base, color);
  widget->setPalette(pal);
}

//----------------------------------------------------------------------------
void qtUIManager::updateArrayTableWidget(
  smtk::attribute::GroupItemPtr dataItem, QTableWidget* widget)
{
  widget->clear();
  widget->setRowCount(0);
  widget->setColumnCount(0);

  if(!dataItem)
    {
    return;
    }

  std::size_t n = dataItem->numberOfGroups();
  std::size_t m = dataItem->numberOfItemsPerGroup();
  if(!m  || !n)
    {
    return;
    }
  smtk::attribute::ValueItemPtr item =dynamic_pointer_cast<ValueItem>(dataItem->item(0));
  if(!item || item->isDiscrete() || item->isExpression())
    {
    return;
    }

  int numCols = static_cast<int>(n*m), numRows = static_cast<int>(item->numberOfValues());
  widget->setColumnCount(numCols);
  widget->setRowCount(numRows);
  for(int h=0; h<numCols; h++)
    {
    QTableWidgetItem *qtablewidgetitem = new QTableWidgetItem();
    widget->setHorizontalHeaderItem(h, qtablewidgetitem);
    }
  for (int j = 0; j < numCols; j++) // expecting one item for each column
    {
    qtUIManager::updateTableColRows(dataItem->item(j), j, widget);
    }
}

//----------------------------------------------------------------------------
void qtUIManager::updateTableColRows(smtk::attribute::ItemPtr dataItem,
    int col, QTableWidget* widget)
{
  smtk::attribute::ValueItemPtr item =dynamic_pointer_cast<ValueItem>(dataItem);
  if(!item || item->isDiscrete() || item->isExpression())
    {
    return;
    }
  int numRows = static_cast<int>(item->numberOfValues());
  widget->setRowCount(numRows);
  QString strValue;
  for(int row=0; row < numRows; row++)
    {
    strValue = item->valueAsString(row).c_str();
    widget->setItem(row, col, new QTableWidgetItem(strValue));
    }
}

//----------------------------------------------------------------------------
void qtUIManager::updateArrayDataValue(
  smtk::attribute::GroupItemPtr dataItem, QTableWidgetItem* item)
{
  if(!dataItem)
    {
    return;
    }
  smtk::attribute::DoubleItemPtr dItem =dynamic_pointer_cast<DoubleItem>(
    dataItem->item(item->column()));
  smtk::attribute::IntItemPtr iItem =dynamic_pointer_cast<IntItem>(
    dataItem->item(item->column()));
  if(dItem)
    {
    dItem->setValue(item->row(), item->text().toDouble());
    }
  else if(iItem)
    {
    iItem->setValue(item->row(), item->text().toInt());
    }
}

//----------------------------------------------------------------------------
bool qtUIManager::getExpressionArrayString(
  smtk::attribute::GroupItemPtr dataItem, QString& strValues)
{
  if(!dataItem || !dataItem->numberOfRequiredGroups())
    {
    return false;
    }
  smtk::attribute::ValueItemPtr item =dynamic_pointer_cast<ValueItem>(dataItem->item(0));
  if(!item || item->isDiscrete() || item->isExpression())
    {
    return false;
    }
  int numberOfComponents = static_cast<int>(dataItem->numberOfItemsPerGroup());
  int nVals = static_cast<int>(item->numberOfValues());
  QStringList strVals;
  smtk::attribute::ValueItemPtr valueitem;
  for(int i=0; i < nVals; i++)
    {
    for(int c=0; c<numberOfComponents-1; c++)
      {
      valueitem =dynamic_pointer_cast<ValueItem>(dataItem->item(c));
      strVals << valueitem->valueAsString(i).c_str() <<"\t";
      }
    valueitem =dynamic_pointer_cast<ValueItem>(dataItem->item(numberOfComponents-1));
    strVals << valueitem->valueAsString(i).c_str();
    strVals << LINE_BREAKER_STRING;
    }
  strValues = strVals.join(" ");
  return true;
}

//----------------------------------------------------------------------------
void qtUIManager::removeSelectedTableValues(
  smtk::attribute::GroupItemPtr dataItem, QTableWidget* table)
{
  if(!dataItem)
    {
    return;
    }

  int numRows = table->rowCount(), numCols = table->columnCount();
  for(int r=numRows-1; r>=0; --r)
    {
    if(table->item(r, 0)->isSelected())
      {
      for(int i = 0; i < numCols; i++)
        {
        smtk::attribute::DoubleItemPtr dItem =dynamic_pointer_cast<DoubleItem>(dataItem->item(i));
        smtk::attribute::IntItemPtr iItem =dynamic_pointer_cast<IntItem>(dataItem->item(i));
        if(dItem)
          {
          dItem->removeValue(r);
          }
        else if(iItem)
          {
          iItem->removeValue(r);
          }
        }
      table->removeRow(r);
      }
    }
}

//----------------------------------------------------------------------------
void qtUIManager::addNewTableValues(smtk::attribute::GroupItemPtr dataItem,
  QTableWidget* table, double* vals, int numVals)
{
  int numCols = table->columnCount();
  if(!dataItem || numCols != numVals)
    {
    return;
    }
  int totalRow = table->rowCount();
  table->setRowCount(++totalRow);

  for(int i=0; i<numVals; i++)
    {
    smtk::attribute::DoubleItemPtr dItem =dynamic_pointer_cast<DoubleItem>(dataItem->item(i));
    smtk::attribute::IntItemPtr iItem =dynamic_pointer_cast<IntItem>(dataItem->item(i));
    if(dItem)
      {
      dItem->appendValue(vals[i]);
      }
    else if(iItem)
      {
      iItem->appendValue(vals[i]);
      }
    QString strValue = QString::number(vals[i]);
    table->setItem(totalRow-1, i, new QTableWidgetItem(strValue));
    }
}
//----------------------------------------------------------------------------
void qtUIManager::onFileItemCreated(qtFileItem* fileItem)
{
  if (m_useInternalFileBrowser)
    {
    fileItem->enableFileBrowser();
    }
  else
    {
    emit this->fileItemCreated(fileItem);
    }
}
//----------------------------------------------------------------------------
void qtUIManager::onModelEntityItemCreated(
  smtk::attribute::qtModelEntityItem* entItem)
{
  emit this->modelEntityItemCreated(entItem);
}
//----------------------------------------------------------------------------
void qtUIManager::onMeshEntityItemCreated(
  smtk::attribute::qtMeshEntityItem* entItem)
{
  emit this->meshEntityItemCreated(entItem);
}

//----------------------------------------------------------------------------
QWidget* qtUIManager::createExpressionRefWidget(
  smtk::attribute::ItemPtr attitem, int elementIdx, QWidget* pWidget, qtBaseView* bview)
{
  smtk::attribute::ValueItemPtr inputitem =dynamic_pointer_cast<ValueItem>(attitem);
  if(!inputitem)
    {
    return NULL;
    }
  smtk::attribute::RefItemPtr item =inputitem->expressionReference(elementIdx);
  if(!item)
    {
    return NULL;
    }

  const RefItemDefinition *itemDef =
    dynamic_cast<const RefItemDefinition*>(item->definition().get());
  smtk::attribute::DefinitionPtr attDef = itemDef->attributeDefinition();
  if(!attDef)
    {
    return NULL;
    }
  QList<QString> attNames;
  std::vector<smtk::attribute::AttributePtr> result;
  System *lAttSystem = attDef->system();
  lAttSystem->findAttributes(attDef, result);
  std::vector<smtk::attribute::AttributePtr>::iterator it;
  for (it=result.begin(); it!=result.end(); ++it)
    {
    attNames.push_back((*it)->name().c_str());
    }

  QComboBox* combo = new QComboBox(pWidget);
  QVariant vdata(elementIdx);
  combo->setProperty("ElementIndex", vdata);
  QVariant vobject;
  vobject.setValue(static_cast<void*>(attitem.get()));
  combo->setProperty("AttItemObj", vobject);
  combo->addItems(attNames);

  int setIndex = -1;
  if (item->isSet(elementIdx))
    {
    setIndex = attNames.indexOf(item->valueAsString(elementIdx).c_str());
    }
  combo->setCurrentIndex(setIndex);
  QVariant viewobject;
  viewobject.setValue(static_cast<void*>(bview));
  combo->setProperty("QtViewObj", viewobject);

  QObject::connect(combo,  SIGNAL(currentIndexChanged(int)),
    this, SLOT(onExpressionReferenceChanged()), Qt::QueuedConnection);

  return combo;
}
//----------------------------------------------------------------------------
void qtUIManager::onExpressionReferenceChanged()
{
  QComboBox* const comboBox = qobject_cast<QComboBox*>(
    QObject::sender());
  if(!comboBox)
    {
    return;
    }
  int curIdx = comboBox->currentIndex();
  int elementIdx = comboBox->property("ElementIndex").toInt();
  ValueItem* inputitem =static_cast<ValueItem*>(
    comboBox->property("AttItemObj").value<void *>());
  if(!inputitem)
    {
    return;
    }
  smtk::attribute::RefItemPtr item =inputitem->expressionReference(elementIdx);
  if(!item)
    {
    return;
    }

  if(curIdx>=0)
    {
    System *lAttSystem = item->attribute()->system();
    AttributePtr attPtr = lAttSystem->findAttribute(comboBox->currentText().toStdString());
    if(elementIdx >=0 && inputitem->isSet(elementIdx) &&
      attPtr == inputitem->expression(elementIdx))
      {
      return; // nothing to do
      }

    if(attPtr)
      {
      inputitem->setExpression(elementIdx, attPtr);
      }
    else
      {
      item->unset(elementIdx);
      inputitem->unset(elementIdx);
      }
    }
  else
    {
    item->unset(elementIdx);
    inputitem->unset(elementIdx);
    }

  qtBaseView* bview =static_cast<qtBaseView*>(
    comboBox->property("QtViewObj").value<void *>());
  if(bview)
    {
    bview->valueChanged(inputitem->pointer());
    }
}
/*
//----------------------------------------------------------------------------
QWidget* qtUIManager::createComboBox(
  smtk::attribute::ItemPtr attitem, int elementIdx, QWidget* pWidget,
  qtBaseView* bview)
{
  smtk::attribute::ValueItemPtr item =dynamic_pointer_cast<ValueItem>(attitem);
  if(!item)
    {
    return NULL;
    }

  const ValueItemDefinition *itemDef =
    dynamic_cast<const ValueItemDefinition*>(item->definition().get());

  QList<QString> discreteVals;
  QString tooltip;
  for (size_t i = 0; i < itemDef->numberOfDiscreteValues(); i++)
    {
    std::string enumText = itemDef->discreteEnum(static_cast<int>(i));
    if(itemDef->hasDefault() &&
      static_cast<size_t>(itemDef->defaultDiscreteIndex()) == i)
      {
      tooltip = "Default: " + QString(enumText.c_str());
      }
    discreteVals.push_back(enumText.c_str());
    }

  QComboBox* combo = new QComboBox(pWidget);
  if(!tooltip.isEmpty())
    {
    combo->setToolTip(tooltip);
    }
  QVariant vdata(elementIdx);
  combo->setProperty("ElementIndex", vdata);
  QVariant vobject;
  vobject.setValue(static_cast<void*>(attitem.get()));
  combo->setProperty("AttItemObj", vobject);
  combo->addItems(discreteVals);
  int setIndex = -1;
  if (item->isSet(elementIdx))
    {
    setIndex = item->discreteIndex(elementIdx);
    }
  if(setIndex < 0 && itemDef->hasDefault() &&
    itemDef->defaultDiscreteIndex() < combo->count())
    {
    setIndex = itemDef->defaultDiscreteIndex();
    }
  combo->setCurrentIndex(setIndex);

  QObject::connect(combo,  SIGNAL(currentIndexChanged(int)),
    this, SLOT(onComboIndexChanged()), Qt::QueuedConnection);

  qtItem* returnItem = new qtComboItem(attitem, elementIdx, pWidget, bview);
  return returnItem ? returnItem->widget() : NULL;
}
*/
/*
//----------------------------------------------------------------------------
void qtUIManager::onComboIndexChanged()
{
  QComboBox* const comboBox = qobject_cast<QComboBox*>(
    QObject::sender());
  if(!comboBox)
    {
    return;
    }
  int curIdx = comboBox->currentIndex();
  int elementIdx = comboBox->property("ElementIndex").toInt();
  ValueItem* item =static_cast<ValueItem*>(
    comboBox->property("AttItemObj").value<void *>());
  if(!item)
    {
    return;
    }
  if(curIdx>=0)
    {
    item->setDiscreteIndex(elementIdx, curIdx);
    }
  else
    {
    item->unset(elementIdx);
    }
}
*/
//----------------------------------------------------------------------------
QWidget* qtUIManager::createInputWidget(
  smtk::attribute::ItemPtr attitem,int elementIdx, QWidget* pWidget,
  qtBaseView* bview, QLayout* childLayout)
{
  smtk::attribute::ValueItemPtr item =dynamic_pointer_cast<ValueItem>(attitem);
  if(!item)
    {
    return NULL;
    }

  return (item->allowsExpressions() /*&& item->isExpression(elementIdx)*/) ?
    this->createExpressionRefWidget(item,elementIdx,pWidget, bview) :
    (item->isDiscrete() ? (new qtDiscreteValueEditor(
      item, elementIdx, pWidget, bview, childLayout)) :
      this->createEditBox(item,elementIdx,pWidget, bview));
}
//----------------------------------------------------------------------------
QWidget* qtUIManager::createEditBox(
  smtk::attribute::ItemPtr attitem,int elementIdx,QWidget* pWidget, qtBaseView* bview)
{
  smtk::attribute::ValueItemPtr item =dynamic_pointer_cast<ValueItem>(attitem);
  if(!item)
    {
    return NULL;
    }

  QWidget* inputWidget = NULL;
  QVariant vdata(elementIdx);
  bool isDefault = false;
  switch (item->type())
    {
    case smtk::attribute::Item::DOUBLE:
      {
      QLineEdit* editBox = new QLineEdit(pWidget);
      const DoubleItemDefinition *dDef =
        dynamic_cast<const DoubleItemDefinition*>(item->definition().get());
      qtDoubleValidator *validator = new qtDoubleValidator(pWidget);
      validator->setUIManager(this);
      editBox->setValidator(validator);
      editBox->setFixedWidth(100);
      QString tooltip;
      double value=smtk_DOUBLE_MIN;
      if(dDef->hasMinRange())
        {
        value = dDef->minRange();
        if(!dDef->minRangeInclusive())
          {
          double multiplier = value >= 0 ? 1. : -1.;
          double to = multiplier*value*1.001+1;
          value = nextafter(value, to);
          }
        validator->setBottom(value);
        QString inclusive = dDef->minRangeInclusive() ? "Inclusive" : "Not Inclusive";
        tooltip.append("Min(").append(inclusive).append("): ").append(QString::number(dDef->minRange()));
        }
      value=smtk_DOUBLE_MAX;
      if(dDef->hasMaxRange())
        {
        value = dDef->maxRange();
        if(!dDef->maxRangeInclusive())
          {
          double multiplier = value >= 0 ? -1. : 1.;
          double to = multiplier*value*1.001-1;
          value = nextafter(value, to);
          }
        validator->setTop(value);
        if(!tooltip.isEmpty())
          {
          tooltip.append("; ");
          }
        QString inclusive = dDef->maxRangeInclusive() ? "Inclusive" : "Not Inclusive";
        tooltip.append("Max(").append(inclusive).append("): ").append(QString::number(dDef->maxRange()));
        }
      if(dDef->hasDefault())
        {
        value = dDef->defaultValue(elementIdx);
        if(!tooltip.isEmpty())
          {
          tooltip.append("; ");
          }
        tooltip.append("Default: ").append(QString::number(value));
        }

      smtk::attribute::DoubleItemPtr ditem =dynamic_pointer_cast<DoubleItem>(item);
      if(item->isSet(elementIdx))
        {
        editBox->setText(item->valueAsString(elementIdx).c_str());
        isDefault = item->isUsingDefault(elementIdx);
        }
      else if(dDef->hasDefault())
        {
        editBox->setText(QString::number(dDef->defaultValue(elementIdx)));
        isDefault = true;
        }
      if(!tooltip.isEmpty())
        {
        editBox->setToolTip(tooltip);
        }
      QVariant tvdata;
      tvdata.setValue(static_cast<void*>(editBox));
      validator->setProperty("MyWidget", tvdata);
      inputWidget = editBox;
      break;
      }
    case smtk::attribute::Item::INT:
      {
      QLineEdit* editBox = new QLineEdit(pWidget);
      const IntItemDefinition *iDef =
        dynamic_cast<const IntItemDefinition*>(item->definition().get());
      qtIntValidator *validator = new qtIntValidator(pWidget);
      validator->setUIManager(this);
      editBox->setValidator(validator);
      editBox->setFixedWidth(100);
      QString tooltip;

      int value=smtk_INT_MIN;
      if(iDef->hasMinRange())
        {
        value = iDef->minRangeInclusive() ?
          iDef->minRange() : iDef->minRange() + 1;
        validator->setBottom(value);
        QString inclusive = iDef->minRangeInclusive() ? "Inclusive" : "Not Inclusive";
        tooltip.append("Min(").append(inclusive).append("): ").append(QString::number(iDef->minRange()));
        }
      value=smtk_INT_MAX;
      if(iDef->hasMaxRange())
        {
        value = iDef->maxRangeInclusive() ?
          iDef->maxRange() : iDef->maxRange() - 1;
        validator->setTop(value);
        if(!tooltip.isEmpty())
          {
          tooltip.append("; ");
          }
        QString inclusive = iDef->minRangeInclusive() ? "Inclusive" : "Not Inclusive";
        tooltip.append("Max(").append(inclusive).append("): ").append(QString::number(iDef->maxRange()));
        }
      if(iDef->hasDefault())
        {
        value = iDef->defaultValue(elementIdx);
        if(!tooltip.isEmpty())
          {
          tooltip.append("; ");
          }
        tooltip.append("Default: ").append(QString::number(value));
        }

      smtk::attribute::IntItemPtr iItem =dynamic_pointer_cast<IntItem>(item);
      if(item->isSet(elementIdx))
        {
        editBox->setText(item->valueAsString(elementIdx).c_str());

        isDefault = iDef->hasDefault() &&
          iDef->defaultValue(elementIdx)==iItem->value(elementIdx);
        }
      else if(iDef->hasDefault())
        {
        editBox->setText(QString::number(iDef->defaultValue(elementIdx)));
        isDefault = true;
        }
      if(!tooltip.isEmpty())
        {
        editBox->setToolTip(tooltip);
        }
      QVariant tvdata;
      tvdata.setValue(static_cast<void*>(editBox));
      validator->setProperty("MyWidget", tvdata);
      inputWidget = editBox;
      break;
      }
    case smtk::attribute::Item::STRING:
      {
      const StringItemDefinition *sDef =
        dynamic_cast<const StringItemDefinition*>(item->definition().get());
      smtk::attribute::StringItemPtr sitem =dynamic_pointer_cast<StringItem>(item);
      QString valText;
      if(item->isSet(elementIdx))
        {
        valText = item->valueAsString(elementIdx).c_str();
        isDefault = sDef->hasDefault() &&
          sDef->defaultValue(elementIdx)==sitem->value(elementIdx);
        }
      else if(sDef->hasDefault())
        {
        valText = sDef->defaultValue(elementIdx).c_str();
        isDefault = true;
        }

      if(sDef->isMultiline())
        {
        qtTextEdit* textEdit = new qtTextEdit(pWidget);
        textEdit->setPlainText(valText);
        QObject::connect(textEdit, SIGNAL(textChanged()),
          this, SLOT(onTextEditChanged()));
        inputWidget = textEdit;
        }
      else
        {
        QLineEdit* lineEdit = new QLineEdit(pWidget);
        lineEdit->setText(valText);
        inputWidget = lineEdit;
        }
//      inputWidget->setMinimumWidth(100);
      inputWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      if(sDef->hasDefault())
        {
        QString tooltip;
        tooltip.append("Default: ").append(sDef->defaultValue(elementIdx).c_str());
        inputWidget->setToolTip(tooltip);
       }
     break;
      }
    default:
      //this->m_errorStatus << "Error: Unsupported Item Type: " <<
      // smtk::attribute::Item::type2String(item->type()) << "\n";
      break;
    }
  if(inputWidget)
    {
    inputWidget->setProperty("ElementIndex", vdata);
    QVariant vobject;
    vobject.setValue(static_cast<void*>(attitem.get()));
    inputWidget->setProperty("AttItemObj", vobject);
//    inputWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    QVariant viewobject;
    viewobject.setValue(static_cast<void*>(bview));
    inputWidget->setProperty("QtViewObj", viewobject);

    this->setWidgetColor(inputWidget,
      isDefault ? this->DefaultValueColor : Qt::white);
    }
  if(QLineEdit* const editBox = qobject_cast<QLineEdit*>(inputWidget))
    {
    QObject::connect(editBox, SIGNAL(textChanged(const QString&)),
      this, SLOT(onLineEditChanged()), Qt::QueuedConnection);
    QObject::connect(editBox, SIGNAL(editingFinished()),
      this, SLOT(onLineEditFinished()), Qt::QueuedConnection);
    }

  return inputWidget;
}

//----------------------------------------------------------------------------
void qtUIManager::onTextEditChanged()
{
  this->onInputValueChanged(QObject::sender());
}

//----------------------------------------------------------------------------
void qtUIManager::onLineEditChanged()
{
  // Here we only handle changes when this is invoked from setText()
  // which is normally used programatically, and the setText() will have
  // modified flag reset to false;
  QLineEdit* const editBox = qobject_cast<QLineEdit*>(QObject::sender());
  if(!editBox)
    {
    return;
    }
  // If this is not from setText(), ignore it. We are using editingFinished
  // signal to handle others.
  if(editBox->isModified())
    {
    return;
    }

  this->onInputValueChanged(editBox);
}

//----------------------------------------------------------------------------
void qtUIManager::onLineEditFinished()
{
  this->onInputValueChanged(QObject::sender());
}

//----------------------------------------------------------------------------
void qtUIManager::onInputValueChanged(QObject* obj)
{
  QLineEdit* const editBox = qobject_cast<QLineEdit*>(obj);
  QTextEdit* const textBox = qobject_cast<QTextEdit*>(obj);
  if(!editBox && !textBox)
    {
    return;
    }
  QWidget* inputBox;
  if(editBox!=NULL)
    {
    inputBox = editBox;
    }
  else
    {
    inputBox = textBox;
    }
  ValueItem* rawitem =static_cast<ValueItem*>(
    inputBox->property("AttItemObj").value<void *>());
  if(!rawitem)
    {
    return;
    }

  int elementIdx = editBox ? editBox->property("ElementIndex").toInt() :
    textBox->property("ElementIndex").toInt();
  bool isDefault = false;
  bool valChanged = false;
  if(editBox && !editBox->text().isEmpty())
    {
    if(rawitem->type()==smtk::attribute::Item::DOUBLE)
      {
      double val = smtk::dynamic_pointer_cast<DoubleItem>(rawitem->pointer())->value(elementIdx);
      if(!(rawitem->isSet(elementIdx)) || val != editBox->text().toDouble())
        {
        smtk::dynamic_pointer_cast<DoubleItem>(rawitem->pointer())
          ->setValue(elementIdx, editBox->text().toDouble());
        valChanged = true;
        }
      const DoubleItemDefinition* def =
        dynamic_cast<const DoubleItemDefinition*>(rawitem->definition().get());
      isDefault = def->hasDefault() && editBox->text().toDouble() == def->defaultValue(elementIdx);
      }
    else if(rawitem->type()==smtk::attribute::Item::INT)
      {
      int val = smtk::dynamic_pointer_cast<IntItem>(rawitem->pointer())->value(elementIdx);
      if(!(rawitem->isSet(elementIdx)) || val != editBox->text().toInt())
        {
        smtk::dynamic_pointer_cast<IntItem>(rawitem->pointer())
          ->setValue(elementIdx, editBox->text().toInt());
        valChanged = true;
        }
      const IntItemDefinition* def =
        dynamic_cast<const IntItemDefinition*>(rawitem->definition().get());
      isDefault = def->hasDefault() && editBox->text().toInt() == def->defaultValue(elementIdx);
      }
    else if(rawitem->type()==smtk::attribute::Item::STRING)
      {
      std::string val = smtk::dynamic_pointer_cast<StringItem>(rawitem->pointer())->value(elementIdx);
      if(!(rawitem->isSet(elementIdx)) || val != editBox->text().toStdString())
        {
        smtk::dynamic_pointer_cast<StringItem>(rawitem->pointer())
          ->setValue(elementIdx, editBox->text().toStdString());
        valChanged = true;
        }
      const StringItemDefinition* def =
        dynamic_cast<const StringItemDefinition*>(rawitem->definition().get());
      isDefault = def->hasDefault() && editBox->text().toStdString() == def->defaultValue(elementIdx);
      }
    else
      {
      rawitem->unset(elementIdx);
      valChanged = true;
      }
    }
  else if(textBox && !textBox->toPlainText().isEmpty() &&
     rawitem->type()==smtk::attribute::Item::STRING)
    {
    std::string val = smtk::dynamic_pointer_cast<StringItem>(rawitem->pointer())->value(elementIdx);
    if(!(rawitem->isSet(elementIdx)) || val != editBox->text().toStdString())
      {
      smtk::dynamic_pointer_cast<StringItem>(rawitem->pointer())
        ->setValue(elementIdx, textBox->toPlainText().toStdString());
      valChanged = true;
      }
    const StringItemDefinition* def =
      dynamic_cast<const StringItemDefinition*>(rawitem->definition().get());
    isDefault = def->hasDefault() && textBox->toPlainText().toStdString() == def->defaultValue(elementIdx);
    }
  else
    {
    rawitem->unset(elementIdx);
    valChanged = true;
    }

  qtBaseView* bview =static_cast<qtBaseView*>(
    inputBox->property("QtViewObj").value<void *>());
  if(bview && valChanged)
    {
    bview->valueChanged(rawitem->pointer());
    }

  this->setWidgetColor(inputBox,
    isDefault ? this->DefaultValueColor : Qt::white);

}
//----------------------------------------------------------------------------
bool qtUIManager::updateTableItemCheckState(
  QTableWidgetItem* labelitem, smtk::attribute::ItemPtr attItem)
{
  bool bEnabled = true;
  if(attItem->definition()->isOptional())
    {
    Qt::CheckState checkState = attItem->isEnabled() ? Qt::Checked :
     (attItem->definition()->isEnabledByDefault() ? Qt::Checked : Qt::Unchecked);
    labelitem->setCheckState(checkState);
    QVariant vdata;
    vdata.setValue(static_cast<void*>(attItem.get()));
    labelitem->setData(Qt::UserRole, vdata);
    labelitem->setFlags(labelitem->flags() | Qt::ItemIsUserCheckable);
    bEnabled = (checkState==Qt::Checked);
    }
  return bEnabled;
}

//----------------------------------------------------------------------------
qtBaseView *qtUIManager::createView(smtk::view::BasePtr smtkView,
  QWidget *pWidget)
{
  qtBaseView *qtView = NULL;  // return value
  switch(smtkView->type())
    {
    case smtk::view::Base::ATTRIBUTE:
      qtView = new qtAttributeView(smtkView, pWidget, this);
      qtUIManager::processAttributeView(qobject_cast<qtAttributeView*>(qtView));
      break;
    case smtk::view::Base::GROUP:
      qtView = new qtGroupView(smtkView, pWidget, this);
      qtUIManager::processGroupView(qobject_cast<qtGroupView*>(qtView));
      break;
    case smtk::view::Base::INSTANCED:
      qtView = new qtInstancedView(smtkView, pWidget, this);
      qtUIManager::processInstancedView(qobject_cast<qtInstancedView*>(qtView));
      break;
    case smtk::view::Base::MODEL_ENTITY:
//      qtView = new qtModelView(smtkView, pWidget, this);
//      qtUIManager::processModelView(qobject_cast<qtModelView*>(qtView));
      break;
    case smtk::view::Base::SIMPLE_EXPRESSION:
      qtView = new qtSimpleExpressionView(smtkView, pWidget, this);
      qtUIManager::processSimpleExpressionView(qobject_cast<qtSimpleExpressionView*>(qtView));
      break;
    default:
      break;
      //this->m_errorStatus << "Unsupport View Type "
      //                    << View::type2String(sec->type()) << "\n";
    }
  return qtView;
}

//----------------------------------------------------------------------------
void qtUIManager::onViewUIModified(smtk::attribute::qtBaseView* bview,
                                   smtk::attribute::ItemPtr item)
{
  emit this->uiChanged(bview, item);
}

//----------------------------------------------------------------------------
int qtUIManager::getWidthOfAttributeMaxLabel(smtk::attribute::DefinitionPtr def,
                                     const QFont &font)
{
  std::string text;
  if(this->Def2LongLabel.contains(def))
    {
    text = this->Def2LongLabel[def];
    }
  else
    {
    this->findDefinitionLongLabel(def, text);
    }

  this->Def2LongLabel[def] = text;
  QFontMetrics fontsize(font);
  return fontsize.width(text.c_str());
}

//----------------------------------------------------------------------------
void qtUIManager::findDefinitionLongLabel(
  smtk::attribute::DefinitionPtr def, std::string &labelText)
{
  QList<smtk::attribute::ItemDefinitionPtr> itemDefs;
  int i, n = def->numberOfItemDefinitions();
  for (i = 0; i < n; i++)
    {
    itemDefs.push_back(def->itemDefinition(i));
    }

  this->getItemsLongLabel(itemDefs, labelText);
}

//----------------------------------------------------------------------------
void qtUIManager::getItemsLongLabel(
  const QList<smtk::attribute::ItemDefinitionPtr>& itemDefs,
  std::string &labelText)
{
  bool hasOptionalItem = false;
  foreach (smtk::attribute::ItemDefinitionPtr itDef, itemDefs)
    {
    smtk::attribute::Item::Type itType = itDef->type();
    // GROUP and VOID type uses their own label length
    if(itType == Item::GROUP || itType == Item::VOID)
      {
      continue;
      }
    std::string text = itDef->label().empty() ?
      itDef->name() : itDef->label();
    if(itDef->isOptional())
      {
      hasOptionalItem = true;
      }
    labelText = (text.length() > labelText.length()) ?
      text : labelText;
    }

  // Add spaces to compensate checkbox width and some spacing.
  labelText += (hasOptionalItem ? "     " : " ");
}

//----------------------------------------------------------------------------
int qtUIManager::getWidthOfItemsMaxLabel(
      const QList<smtk::attribute::ItemDefinitionPtr>& itemDefs,
      const QFont &font)
{
  std::string text;
  this->getItemsLongLabel(itemDefs, text);
  QFontMetrics fontsize(font);
  return fontsize.width(text.c_str());
}

//----------------------------------------------------------------------------
void qtUIManager::findDefinitionsLongLabels()
{
  this->Def2LongLabel.clear();
  // Generate list of all concrete definitions in the manager
  std::vector<smtk::attribute::DefinitionPtr> defs;
  std::vector<smtk::attribute::DefinitionPtr> baseDefinitions;
  this->m_AttSystem.findBaseDefinitions(baseDefinitions);
  std::vector<smtk::attribute::DefinitionPtr>::const_iterator baseIter;

  for (baseIter = baseDefinitions.begin();
       baseIter != baseDefinitions.end();
       baseIter++)
    {
    std::vector<smtk::attribute::DefinitionPtr> derivedDefs;
    m_AttSystem.findAllDerivedDefinitions(*baseIter, true, derivedDefs);
    defs.insert(defs.end(), derivedDefs.begin(), derivedDefs.end());
    }

  std::vector<smtk::attribute::DefinitionPtr>::const_iterator defIter;
  for (defIter = defs.begin(); defIter != defs.end(); defIter++)
    {
    std::string text;
    this->findDefinitionLongLabel(*defIter, text);
    this->Def2LongLabel[*defIter] = text;
    }
}
