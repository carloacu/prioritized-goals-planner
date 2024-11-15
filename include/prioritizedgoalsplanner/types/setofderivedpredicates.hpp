#ifndef INCLUDE_PRIORITIZEDGOALSPLANNER_TYPES_SETOFDERIVEDPREDICATES_HPP
#define INCLUDE_PRIORITIZEDGOALSPLANNER_TYPES_SETOFDERIVEDPREDICATES_HPP

#include "../util/api.hpp"
#include <map>
#include <memory>
#include <string>
#include <prioritizedgoalsplanner/types/derivedpredicate.hpp>

namespace pgp
{
struct Condition;
struct Predicate;


struct PRIORITIZEDGOALSPLANNER_API SetOfDerivedPredicates
{
  SetOfDerivedPredicates();

  void addAll(const SetOfDerivedPredicates& pOther);
  void addDerivedPredicate(const DerivedPredicate& pDerivedPredicate);

  const Predicate* nameToPredicatePtr(const std::string& pPredicateName) const;

  std::unique_ptr<Condition> optFactToConditionPtr(const FactOptional& pFactOptional) const;


private:
  std::map<std::string, DerivedPredicate> _nameToDerivedPredicate;
};

} // namespace pgp

#endif // INCLUDE_PRIORITIZEDGOALSPLANNER_TYPES_SETOFDERIVEDPREDICATES_HPP
