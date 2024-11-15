#include "converttoparallelplan.hpp"
#include <list>
#include <prioritizedgoalsplanner/types/action.hpp>
#include <prioritizedgoalsplanner/types/actioninvocationwithgoal.hpp>
#include <prioritizedgoalsplanner/types/domain.hpp>
#include <prioritizedgoalsplanner/types/lookforanactionoutputinfos.hpp>
#include <prioritizedgoalsplanner/types/problem.hpp>
#include <prioritizedgoalsplanner/types/worldstate.hpp>
#include <prioritizedgoalsplanner/prioritizedgoalsplanner.hpp>
#include "notifyactiondone.hpp"

namespace pgp
{

namespace
{

std::list<pgp::Goal> _checkSatisfiedGoals(
    const Problem& pProblem,
    const Domain& pDomain,
    const std::list<std::list<pgp::ActionInvocationWithGoal>>& pPlan,
    const pgp::ActionInvocationWithGoal* pActionToDoFirstPtr,
    const pgp::ActionInvocationWithGoal* pActionToSkipPtr,
    const std::unique_ptr<std::chrono::steady_clock::time_point>& pNow)
{
  std::list<pgp::Goal> res;
  auto problem = pProblem;
  const auto& actions = pDomain.actions();

  LookForAnActionOutputInfos lookForAnActionOutputInfos;
  if (pActionToDoFirstPtr != nullptr)
  {
    bool goalChanged = false;
    updateProblemForNextPotentialPlannerResult(problem, goalChanged, *pActionToDoFirstPtr, pDomain, pNow, nullptr,
                                               &lookForAnActionOutputInfos);
  }

  for (const auto& currActionsToDoInParallel : pPlan)
  {
    for (const auto& currAction : currActionsToDoInParallel)
    {
      if (&currAction == pActionToSkipPtr)
        continue;

      auto itAction = actions.find(currAction.actionInvocation.actionId);
      if (itAction != actions.end() &&
          (!itAction->second.precondition ||
           itAction->second.precondition->isTrue(problem.worldState)))
      {
        bool goalChanged = false;
        updateProblemForNextPotentialPlannerResult(problem, goalChanged, currAction, pDomain, pNow, nullptr,
                                                    &lookForAnActionOutputInfos);
      }
      else
      {
        return {};
      }
    }
  }

  lookForAnActionOutputInfos.moveGoalsDone(res);
  return res;
}


}



std::list<std::list<pgp::ActionInvocationWithGoal>> toParallelPlan
(std::list<pgp::ActionInvocationWithGoal>& pSequentialPlan,
 bool pParalleliseOnyFirstStep,
 const Problem& pProblem,
 const Domain& pDomain,
 const std::list<pgp::Goal>& pGoals,
 const std::unique_ptr<std::chrono::steady_clock::time_point>& pNow)
{
  std::list<std::list<pgp::ActionInvocationWithGoal>> res;
  auto currentProblem = pProblem;

  // Connvert list of actions to a list of list of actions
  while (!pSequentialPlan.empty())
  {
    res.emplace_back(1, std::move(pSequentialPlan.front()));
    pSequentialPlan.pop_front();
  }

  const auto& actions = pDomain.actions();
  for (auto itPlanStep = res.begin(); itPlanStep != res.end(); ++itPlanStep)
  {
    for (auto& currAction : *itPlanStep)
      notifyActionStarted(currentProblem, pDomain, currAction, pNow);

    auto itPlanStepCandidate = itPlanStep;
    ++itPlanStepCandidate;
    while (itPlanStepCandidate != res.end())
    {
      if (!itPlanStepCandidate->empty())
      {
        auto& actionInvocationCand = itPlanStepCandidate->front();
        auto itActionCand = actions.find(actionInvocationCand.actionInvocation.actionId);
        if (itActionCand != actions.end() &&
            (!itActionCand->second.precondition ||
             itActionCand->second.precondition->isTrue(currentProblem.worldState)))
        {
          auto goalsCand = _checkSatisfiedGoals(currentProblem, pDomain, res, &actionInvocationCand, &actionInvocationCand, pNow);
          if (goalsCand == pGoals)
          {
            itPlanStep->emplace_back(std::move(actionInvocationCand));
            itPlanStepCandidate = res.erase(itPlanStepCandidate);
            break;
          }
        }
      }
      ++itPlanStepCandidate;
    }

    if (pParalleliseOnyFirstStep)
      break;
  }

  return res;
}



} // End of namespace pgp
