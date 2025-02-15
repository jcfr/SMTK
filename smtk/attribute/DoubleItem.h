//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
// .NAME DoubleItem.h -
// .SECTION Description
// .SECTION See Also

#ifndef smtk_attribute_DoubleItem_h
#define smtk_attribute_DoubleItem_h

#include "smtk/CoreExports.h"
#include "smtk/attribute/ValueItemTemplate.h"

namespace smtk
{
namespace attribute
{
class Attribute;
class DoubleItemDefinition;
class SMTKCORE_EXPORT DoubleItem : public ValueItemTemplate<double>
{
  friend class DoubleItemDefinition;

public:
  smtkTypeMacro(smtk::attribute::DoubleItem);
  ~DoubleItem() override;
  Item::Type type() const override;
  // Assigns this item to be equivalent to another.  Options are processed by derived item classes
  // Returns true if success and false if a problem occured.  By default, an attribute being used by this
  // to represent an expression will be copied if needed.  Use IGNORE_EXPRESSIONS option to prevent this
  // When an expression attribute is copied, its model associations are by default not.
  // Use COPY_MODEL_ASSOCIATIONS if you want them copied as well.These options are defined in Item.h .
  bool assign(smtk::attribute::ConstItemPtr& sourceItem, unsigned int options = 0) override;

protected:
  DoubleItem(Attribute* owningAttribute, int itemPosition);
  DoubleItem(Item* owningItem, int myPosition, int mySubGroupPosition);

private:
};
} // namespace attribute
} // namespace smtk

#endif /* smtk_attribute_DoubleItem_h */
