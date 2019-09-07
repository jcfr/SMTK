//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/model/Instance.h"

#include "smtk/model/Arrangement.h"
#include "smtk/model/EntityRefArrangementOps.h"
#include "smtk/model/PointLocatorExtension.h"
#include "smtk/model/Resource.h"

#include <random>

namespace smtk
{
namespace model
{

EntityRef Instance::prototype() const
{
  return EntityRefArrangementOps::firstRelation<EntityRef>(*this, INSTANCE_OF);
}

bool Instance::setPrototype(const EntityRef& proto)
{
  EntityRef current = EntityRefArrangementOps::firstRelation<EntityRef>(*this, INSTANCE_OF);
  if (proto == current)
  {
    return false;
  }

  auto rec = this->entityRecord();
  auto arr = this->findArrangement(INSTANCE_OF, 0);
  if (arr)
  {
    rec->unarrange(INSTANCE_OF, 0, true);
  }
  rec->modelResource()->addDualArrangement(
    proto.entity(), this->entity(), INSTANCED_BY, 0, UNDEFINED);
  return true;
}

static void GenerateTabularTessellation(Instance& inst, Tessellation* placements)
{
  if (!inst.hasFloatProperties())
  {
    return;
  }
  const FloatData& fprops(inst.floatProperties());
  auto posn = fprops.find("placements");
  int numPosn = static_cast<int>(posn->second.size() / 3);
  for (int ii = 0; ii < numPosn; ++ii)
  {
    placements->addPoint(&posn->second[3 * ii]);
  }
}

static void GenerateRandomTessellation(Instance& inst, Tessellation* placements)
{
  if (!inst.hasFloatProperties() || !inst.hasIntegerProperties())
  {
    return;
  }
  const FloatData& fprops(inst.floatProperties());
  const IntegerData& iprops(inst.integerProperties());
  auto numPts = iprops.find("sample size");
  auto seed = iprops.find("seed");
  auto voi = fprops.find("voi");
  if (numPts == iprops.end() || numPts->second.size() != 1 || voi == fprops.end() ||
    voi->second.size() != 6 || (seed != iprops.end() && seed->second.size() != 1))
  {
    return;
  }
  if (seed == iprops.end())
  {
    // Generate and store a seed
    std::random_device rd;
    inst.setIntegerProperty("seed", static_cast<IntegerList::value_type>(rd()));
    seed = iprops.find("seed");
  }
  int npts = numPts->second[0];
  unsigned iseed = static_cast<unsigned>(seed->second[0]);
  std::mt19937 gen(iseed);
  std::uniform_real_distribution<> distribX(voi->second[0], voi->second[1]);
  std::uniform_real_distribution<> distribY(voi->second[2], voi->second[3]);
  std::uniform_real_distribution<> distribZ(voi->second[4], voi->second[5]);
  for (auto ii = 0; ii < npts; ++ii)
  {
    double pt[3] = { distribX(gen), distribY(gen), distribZ(gen) };
    placements->addPoint(pt);
  }
}

static void SnapPlacementsTo(const Instance& inst, const EntityRefs& snaps, Tessellation* tess)
{
  if (snaps.empty() || !snaps.begin()->isValid() || !inst.isValid())
  {
    return;
  }
  else if (snaps.size() > 1)
  {
    smtkWarningMacro(inst.resource()->log(), "Expected a single model entity to snap to, got "
        << snaps.size() << ". "
        << "Ignoring all but first (" << snaps.begin()->name() << ")");
  }

  std::string snapRule;
  if (inst.hasStringProperty("snap rule"))
  {
    snapRule = inst.stringProperty("snap rule")[0];
  }
  if (snapRule.empty())
  {
    smtkWarningMacro(inst.resource()->log(), "No rule for how to perform snap.");
    return;
  }
  auto snapper = smtk::common::Extension::findAs<PointLocatorExtension>(snapRule);
  if (!snapper)
  {
    smtkErrorMacro(inst.resource()->log(), "Could not create object (" << snapRule << ")"
                                                                       << " to perform snapping.");
    return;
  }
  snapper->closestPointOn(*snaps.begin(), tess->coords(), tess->coords());
}

static void ComputeBounds(Tessellation* tess, const std::vector<double>& pbox, double bbox[6])
{
  // Compute bbox of points in tessellation
  (void)tess;
  (void)bbox;
  // Add pbox min to bbox min, pbox max to bbox max
  (void)pbox;
}

// TODO: This is hardcoded for now, but should allow for lambdas to be registered
// as rules and invoked to generate the tessellation.
Tessellation* Instance::generateTessellation()
{
  std::string rule;
  EntityRef proto;
  if (!this->isValid() || (rule = this->rule()).empty() || !(proto = this->prototype()).isValid())
  {
    return nullptr;
  }

  Tessellation* tess = this->resetTessellation();
  if (rule == "tabular")
  {
    GenerateTabularTessellation(*this, tess);
  }
  else if (rule == "uniform random")
  {
    GenerateRandomTessellation(*this, tess);
  }

  EntityRefs snaps = this->snapEntities();
  SnapPlacementsTo(*this, snaps, tess);

  std::vector<double> pbox = proto.boundingBox();
  double bbox[6];
  ComputeBounds(tess, pbox, bbox);
  this->setBoundingBox(bbox);
  return tess;
}

std::string Instance::rule() const
{
  if (!this->hasStringProperties())
  {
    return "empty";
  }
  const StringData& sprops(this->stringProperties());
  auto rprop = sprops.find("rule");
  return (rprop == sprops.end() || rprop->second.empty()) ? "empty" : rprop->second[0];
}

bool Instance::setRule(const std::string& nextRule)
{
  // Only accept valid rules:
  if (nextRule != "tabular" && nextRule != "uniform random")
  {
    return false;
  }
  // Only place rules on instance entities:
  if (!this->isValid())
  {
    return false;
  }
  // Only change the rule if we need to:
  std::string currRule = this->rule();
  if (currRule != nextRule)
  {
    this->setStringProperty("rule", nextRule);
    return true;
  }
  return false;
}

EntityRefs Instance::snapEntities() const
{
  EntityRefs result;
  // TODO: This is a total hack for now. We should use Arrangements but
  // there's not really a good one for this use case.
  if (!this->hasIntegerProperties())
  {
    return result;
  }
  const IntegerData& iprops(this->integerProperties());
  auto iprop = iprops.find("snap to entities");
  if (iprop == iprops.end() || iprop->second.size() < 8)
  {
    return result;
  }
  smtk::model::Resource::Ptr resource = this->resource();
  int numEnts = static_cast<int>(iprop->second.size() / 8);
  IntegerList::const_iterator pit = iprop->second.begin();
  for (int ii = 0; ii < numEnts; ++ii)
  {
    smtk::common::UUID::value_type data[smtk::common::UUID::SIZE];
    for (int jj = 0; jj < 8; ++jj, ++pit)
    {
      data[2 * jj] = *pit % 256;
      data[2 * jj + 1] = (*pit / 256) % 256;
    }
    result.insert(EntityRef(resource, smtk::common::UUID(data, data + smtk::common::UUID::SIZE)));
  }
  return result;
}

bool Instance::addSnapEntity(const EntityRef& snapTo)
{
  EntityRefs curSnaps = this->snapEntities();
  if (curSnaps.insert(snapTo).second)
  {
    this->setSnapEntities(curSnaps);
    return true;
  }
  return false;
}

bool Instance::removeSnapEntity(const EntityRef& snapTo)
{
  EntityRefs curSnaps = this->snapEntities();
  if (curSnaps.erase(snapTo) > 0)
  {
    this->setSnapEntities(curSnaps);
    return true;
  }
  return false;
}

bool Instance::setSnapEntity(const EntityRef& snapTo)
{
  EntityRefs snapSet;
  snapSet.insert(snapTo);
  return this->setSnapEntities(snapSet);
}

bool Instance::setSnapEntities(const EntityRefs& snapTo)
{
  if (!this->isValid())
  {
    return false;
  }
  if (!this->hasIntegerProperties() && snapTo.empty())
  {
    return false;
  }
  IntegerData& iprops(this->integerProperties());
  auto iprop = iprops.find("snap to entities");
  if (snapTo.empty())
  {
    if (iprop == iprops.end())
    {
      return false;
    }
    else
    {
      iprops.erase("snap to entities");
      return true;
    }
  }
  IntegerList replacement;
  replacement.resize(8 * snapTo.size());
  int numEnts = static_cast<int>(snapTo.size());
  EntityRefs::const_iterator sit = snapTo.begin();
  IntegerList::iterator rit = replacement.begin();
  for (int ii = 0; ii < numEnts; ++ii, ++sit)
  {
    smtk::common::UUID uid(sit->entity());
    smtk::common::UUID::const_iterator uit = uid.begin();
    for (int jj = 0; jj < 8; ++jj, ++rit)
    {
      *rit = uit[2 * jj] + 256 * uit[2 * jj + 1];
    }
  }
  if (iprop == iprops.end())
  {
    iprops.insert(std::make_pair(std::string("snap to entities"), replacement));
  }
  else
  {
    if (replacement == iprop->second)
    {
      return false;
    }
    iprop->second = replacement;
  }
  return true;
}

std::size_t Instance::numberOfPlacements()
{
  const Tessellation* tess = this->hasTessellation();
  return tess ? tess->coords().size() / 3 : 0;
}

bool Instance::isClone() const
{
  // We could also check whether this instance has
  // a SUBSET_OF relation to another instance:
  return this->hasIntegerProperty(Instance::subset);
}

bool Instance::checkMergeable(const Instance& other) const
{
  // TODO: Expand this to include checks on properties like color, mask, scaling, ...
  bool protoMatch = (other.prototype() == this->prototype());
  bool snapsMatch = (other.snapEntities() == this->snapEntities());
  return protoMatch && snapsMatch;
  /*
    (other.prototype() == this->prototype()) &&
    (other.snapEntities() == this->snapEntities());
    */
}

bool Instance::mergeInternal(const Instance& other)
{
  // I. Determine sizes (placements) of instances involved
  auto& places = this->floatProperty(Instance::placements);
  const Tessellation* otherTess = other.hasTessellation();
  const auto& otherCoords = otherTess->coords();
  std::size_t numOtherPlacements = otherCoords.size() / 3;
  std::size_t numThisPlacements = places.size() / 3;

  // II. Determine which optional tabular properties are present
  //     on this instance and other.
  bool hasOrientation = this->hasFloatProperty(Instance::orientations) &&
    !this->floatProperty(Instance::orientations).empty();
  bool hasScales =
    this->hasFloatProperty(Instance::scales) && !this->floatProperty(Instance::scales).empty();
  bool hasMasks =
    this->hasFloatProperty(Instance::masks) && !this->floatProperty(Instance::masks).empty();
  bool hasColors =
    this->hasFloatProperty(Instance::colors) && !this->floatProperty(Instance::colors).empty();
  bool otherHasOrientation = other.hasFloatProperty(Instance::orientations) &&
    !other.floatProperty(Instance::orientations).empty();
  bool otherHasScales =
    other.hasFloatProperty(Instance::scales) && !other.floatProperty(Instance::scales).empty();
  bool otherHasMasks =
    other.hasFloatProperty(Instance::masks) && !other.floatProperty(Instance::masks).empty();
  bool otherHasColors =
    other.hasFloatProperty(Instance::colors) && !other.floatProperty(Instance::colors).empty();

  // III. Append \a other's tessellation coordinates to this instance's placements.
  //
  // Note that this works out because _this_ is required to have its rule
  // set to "tabular" while the _other_ instance may not but will always
  // have coordinates in its tessellation. The other fields (orientation,
  // masks, etc) are only present for tabular rules.
  places.insert(places.end(), otherCoords.begin(), otherCoords.end());

  // IV. Now pad this instance's properties as required to support those
  //     which are present on the other instance but not this one:
  if (!hasOrientation && otherHasOrientation)
  {
    // Pad this instance with default orientation data.
    this->floatProperty(Instance::orientations).resize(numThisPlacements * 3, 0.0);
  }
  if (!hasScales && otherHasScales)
  {
    // Pad this instance with default scaling data.
    this->floatProperty(Instance::scales).resize(numThisPlacements * 3, 1.0);
  }
  if (!hasMasks && otherHasMasks)
  {
    // Pad this instance with default masking data.
    this->floatProperty(Instance::masks).resize(numThisPlacements, 0.0);
  }
  if (!hasColors && otherHasColors)
  {
    // Pad this instance with default scaling data.
    this->floatProperty(Instance::colors).resize(numThisPlacements * 4, 1.0);
  }

  // V. Finally, append properties present from the other instance
  //    or pad if they are needed but not present.
  if (otherHasOrientation)
  {
    const FloatList& otherOrient(other.floatProperty(Instance::orientations));
    FloatList& thisOrient = this->floatProperty(Instance::orientations);
    thisOrient.insert(thisOrient.end(), otherOrient.begin(), otherOrient.end());
  }
  else if (hasOrientation)
  {
    FloatList otherOrient(numOtherPlacements * 3, 0.0);
    FloatList& thisOrient = this->floatProperty(Instance::orientations);
    thisOrient.insert(thisOrient.end(), otherOrient.begin(), otherOrient.end());
  }

  return true;
}

} // namespace model
} // namespace smtk
