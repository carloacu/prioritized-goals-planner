#include <prioritizedgoalsplanner/types/problemmodification.hpp>
#include <prioritizedgoalsplanner/util/util.hpp>


namespace pgp
{

bool ProblemModification::operator==(const ProblemModification& pOther) const
{
  return areUPtrEqual(worldStateModification, pOther.worldStateModification) &&
      areUPtrEqual(potentialWorldStateModification, pOther.potentialWorldStateModification) &&
      areUPtrEqual(worldStateModificationAtStart, pOther.worldStateModificationAtStart) &&
      goalsToAdd == pOther.goalsToAdd &&
      goalsToAddInCurrentPriority == pOther.goalsToAddInCurrentPriority;
}


std::set<FactOptional> ProblemModification::getAllOptFactsThatCanBeModified() const
{
  std::set<FactOptional> res;
  if (worldStateModification)
  {
    worldStateModification->forAllThatCanBeModified([&](const FactOptional& pFactOptional) {
        res.insert(pFactOptional);
        return ContinueOrBreak::CONTINUE;
    });
  }
  if (potentialWorldStateModification)
  {
    potentialWorldStateModification->forAllThatCanBeModified([&](const FactOptional& pFactOptional) {
        res.insert(pFactOptional);
        return ContinueOrBreak::CONTINUE;
    });
  }
  return res;
}

} // !pgp
