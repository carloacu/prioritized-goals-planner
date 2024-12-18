#include <orderedgoalsplanner/types/axiom.hpp>
#include <orderedgoalsplanner/types/event.hpp>
#include <orderedgoalsplanner/types/worldstatemodification.hpp>
#include <orderedgoalsplanner/util/serializer/deserializefrompddl.hpp>

namespace ogp
{

Axiom::Axiom(std::unique_ptr<Condition> pContext,
             const Fact& pImplies,
             const std::vector<Parameter>& pVars)
  : vars(pVars),
    context(pContext ? std::move(pContext) : std::unique_ptr<Condition>()),
    implies(pImplies)
{
}


std::list<Event> Axiom::toEvents(const Ontology& pOntology,
                                 const SetOfEntities& pEntities) const
{
  std::list<Event> res;
  {
    std::size_t pos = 0;
    res.emplace_back(context->clone(), pddlToWsModification(implies.toPddl(true), pos, pOntology, pEntities, vars), vars);
  }
  {
    std::size_t pos = 0;
    FactOptional impliesNegated(implies, true);
    res.emplace_back(context->clone(nullptr, true), pddlToWsModification(impliesNegated.toPddl(true), pos, pOntology, pEntities, vars), vars);
  }
  return res;
}


} // !ogp
