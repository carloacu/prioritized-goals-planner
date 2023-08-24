#include <contextualplanner/contextualplanner.hpp>
#include <algorithm>
#include <contextualplanner/types/factsalreadychecked.hpp>
#include <contextualplanner/types/setofinferences.hpp>
#include <contextualplanner/util/util.hpp>

namespace cp
{

namespace
{

enum class PossibleEffect
{
  SATISFIED,
  SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD,
  NOT_SATISFIED
};

PossibleEffect _merge(PossibleEffect pEff1,
                      PossibleEffect pEff2)
{
  if (pEff1 == PossibleEffect::SATISFIED ||
      pEff2 == PossibleEffect::SATISFIED)
    return PossibleEffect::SATISFIED;
  if (pEff1 == PossibleEffect::SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD ||
      pEff2 == PossibleEffect::SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD)
    return PossibleEffect::SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD;
  return PossibleEffect::NOT_SATISFIED;
}

struct PotentialNextAction
{
  PotentialNextAction()
    : actionId(""),
      actionPtr(nullptr),
      parameters(),
      satisfyObjective(false)
  {
  }
  PotentialNextAction(const ActionId& pActionId,
                      const Action& pAction);

  ActionId actionId;
  const Action* actionPtr;
  std::map<std::string, std::set<std::string>> parameters;
  bool satisfyObjective;

  bool isMoreImportantThan(const PotentialNextAction& pOther,
                           const Problem& pProblem,
                           const Historical* pGlobalHistorical) const;
};


PotentialNextAction::PotentialNextAction(const ActionId& pActionId,
                                         const Action& pAction)
  : actionId(pActionId),
    actionPtr(&pAction),
    parameters(),
    satisfyObjective(false)
{
  for (const auto& currParam : pAction.parameters)
    parameters[currParam];
}


void _getPrecoditionStatistics(std::size_t& nbOfPreconditionsSatisfied,
                               std::size_t& nbOfPreconditionsNotSatisfied,
                               const Action& pAction,
                               const std::set<Fact>& pFacts)
{
  auto onFact = [&](const FactOptional& pFactOptional)
  {
    if (pFactOptional.isFactNegated)
    {
      if (pFacts.count(pFactOptional.fact) == 0)
        ++nbOfPreconditionsSatisfied;
      else
        ++nbOfPreconditionsNotSatisfied;
    }
    else
    {
      if (pFacts.count(pFactOptional.fact) > 0)
        ++nbOfPreconditionsSatisfied;
      else
        ++nbOfPreconditionsNotSatisfied;
    }
  };

  if (pAction.preferInContext)
    pAction.preferInContext->forAll(onFact, [](const Expression&) {});
}


bool PotentialNextAction::isMoreImportantThan(const PotentialNextAction& pOther,
                                              const Problem& pProblem,
                                              const Historical* pGlobalHistorical) const
{
  if (actionPtr == nullptr)
    return false;
  if (pOther.actionPtr == nullptr)
    return true;

  if (satisfyObjective != pOther.satisfyObjective)
    return satisfyObjective;

  auto& action = *actionPtr;
  std::size_t nbOfPreconditionsSatisfied = 0;
  std::size_t nbOfPreconditionsNotSatisfied = 0;
  _getPrecoditionStatistics(nbOfPreconditionsSatisfied, nbOfPreconditionsNotSatisfied, action, pProblem.facts());
  auto& otherAction = *pOther.actionPtr;
  std::size_t otherNbOfPreconditionsSatisfied = 0;
  std::size_t otherNbOfPreconditionsNotSatisfied = 0;
  _getPrecoditionStatistics(otherNbOfPreconditionsSatisfied, otherNbOfPreconditionsNotSatisfied, otherAction, pProblem.facts());
  if (nbOfPreconditionsSatisfied != otherNbOfPreconditionsSatisfied)
    return nbOfPreconditionsSatisfied > otherNbOfPreconditionsSatisfied;
  if (nbOfPreconditionsNotSatisfied != otherNbOfPreconditionsNotSatisfied)
    return nbOfPreconditionsNotSatisfied < otherNbOfPreconditionsNotSatisfied;

  auto nbOfTimesAlreadyDone = pProblem.historical.getNbOfTimeAnActionHasAlreadyBeenDone(actionId);
  auto otherNbOfTimesAlreadyDone = pProblem.historical.getNbOfTimeAnActionHasAlreadyBeenDone(pOther.actionId);
  if (nbOfTimesAlreadyDone != otherNbOfTimesAlreadyDone)
    return nbOfTimesAlreadyDone < otherNbOfTimesAlreadyDone;

  if (pGlobalHistorical != nullptr)
  {
    nbOfTimesAlreadyDone = pGlobalHistorical->getNbOfTimeAnActionHasAlreadyBeenDone(actionId);
    otherNbOfTimesAlreadyDone = pGlobalHistorical->getNbOfTimeAnActionHasAlreadyBeenDone(pOther.actionId);
    if (nbOfTimesAlreadyDone != otherNbOfTimesAlreadyDone)
      return nbOfTimesAlreadyDone < otherNbOfTimesAlreadyDone;
  }

  return actionId < pOther.actionId;
}


bool _lookForAPossibleEffect(bool& pSatisfyObjective,
                             std::map<std::string, std::set<std::string>>& pParameters,
                             const WorldModification& pEffectToCheck,
                             const Goal &pGoal,
                             const Problem& pProblem,
                             const FactOptional& pFactOptionalToSatisfy,
                             const Domain& pDomain,
                             FactsAlreadyChecked& pFactsAlreadychecked);


PossibleEffect _lookForAPossibleDeduction(const std::vector<std::string>& pParameters,
                                          const std::unique_ptr<FactCondition>& pCondition,
                                          const WorldModification& pEffect,
                                          const Fact& pFact,
                                          bool pIsFactNegated,
                                          std::map<std::string, std::set<std::string>>& pParentParameters,
                                          const Goal& pGoal,
                                          const Problem& pProblem,
                                          const FactOptional& pFactOptionalToSatisfy,
                                          const Domain& pDomain,
                                          FactsAlreadyChecked& pFactsAlreadychecked)
{
  if (!pCondition ||
      (pCondition->containsFact(pFact, pIsFactNegated, pParentParameters, pParameters) &&
       pCondition->canBecomeTrue(pProblem)))
  {
    std::map<std::string, std::set<std::string>> parametersToValues;
    for (const auto& currParam : pParameters)
      parametersToValues[currParam];
    bool satisfyObjective = false;
    if (_lookForAPossibleEffect(satisfyObjective, parametersToValues, pEffect, pGoal, pProblem, pFactOptionalToSatisfy,
                                pDomain, pFactsAlreadychecked))
    {
      bool actionIsAPossibleFollowUp = true;
      // fill parent parameters
      for (auto& currParentParam : pParentParameters)
      {
        if (currParentParam.second.empty())
        {
          pCondition->untilFalse(
                [&](const FactOptional& pFactOptional)
          {
            auto parentParamValue = pFact.tryToExtractParameterValueFromExemple(currParentParam.first, pFactOptional.fact);
            if (parentParamValue.empty())
              return true;
            // Maybe the extracted parameter is also a parameter so we replace by it's value
            auto itParam = parametersToValues.find(parentParamValue);
            if (itParam != parametersToValues.end())
              currParentParam.second = itParam->second;
            else
              currParentParam.second.insert(parentParamValue);
            return currParentParam.second.empty();
          },
          [](const Expression&) { return true; }, pProblem, parametersToValues);

          if (!currParentParam.second.empty())
            break;
          if (currParentParam.second.empty())
          {
            actionIsAPossibleFollowUp = false;
            break;
          }
        }
      }
      // Check that the new fact pattern is not already satisfied
      if (actionIsAPossibleFollowUp)
      {
        if (!pProblem.isFactPatternSatisfied(pFact, pIsFactNegated, {}, {}, &pParentParameters, nullptr))
          return PossibleEffect::SATISFIED;
        return PossibleEffect::SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD;
      }
    }
  }
  return PossibleEffect::NOT_SATISFIED;
}

PossibleEffect _lookForAPossibleExistingOrNotFactFromActions(
    const Fact& pFact,
    bool pIsFactNegated,
    std::map<std::string, std::set<std::string>>& pParentParameters,
    const std::map<std::string, std::set<ActionId>>& pPreconditionToActions,
    const Goal& pGoal,
    const Problem& pProblem,
    const FactOptional& pFactOptionalToSatisfy,
    const Domain& pDomain,
    FactsAlreadyChecked& pFactsAlreadychecked)
{
  auto res = PossibleEffect::NOT_SATISFIED;
  auto it = pPreconditionToActions.find(pFact.name);
  if (it != pPreconditionToActions.end())
  {
    auto& actions = pDomain.actions();
    for (const auto& currActionId : it->second)
    {
      auto itAction = actions.find(currActionId);
      if (itAction != actions.end())
      {
        auto& action = itAction->second;
        res = _merge(_lookForAPossibleDeduction(action.parameters, action.precondition, action.effect,
                                                pFact, pIsFactNegated, pParentParameters, pGoal, pProblem, pFactOptionalToSatisfy,
                                                pDomain, pFactsAlreadychecked), res);
        if (res == PossibleEffect::SATISFIED)
          return res;
      }
    }
  }
  return res;
}


PossibleEffect _lookForAPossibleExistingOrNotFactFromInferences(
    const Fact& pFact,
    bool pIsFactNegated,
    std::map<std::string, std::set<std::string>>& pParentParameters,
    const std::map<std::string, std::set<InferenceId>>& pConditionToInferences,
    const std::map<InferenceId, Inference>& pInferences,
    const Goal& pGoal,
    const Problem& pProblem,
    const FactOptional& pFactOptionalToSatisfy,
    const Domain& pDomain,
    FactsAlreadyChecked& pFactsAlreadychecked)
{
  auto res = PossibleEffect::NOT_SATISFIED;
  auto it = pConditionToInferences.find(pFact.name);
  if (it != pConditionToInferences.end())
  {
    for (const auto& currInferenceId : it->second)
    {
      auto itInference = pInferences.find(currInferenceId);
      if (itInference != pInferences.end())
      {
        auto& inference = itInference->second;
        if (inference.factsToModify)
        {
          res = _merge(_lookForAPossibleDeduction(inference.parameters, inference.condition, inference.factsToModify->clone(nullptr),
                                                  pFact, pIsFactNegated, pParentParameters, pGoal, pProblem, pFactOptionalToSatisfy,
                                                  pDomain, pFactsAlreadychecked), res);
          if (res == PossibleEffect::SATISFIED)
            return res;
        }
      }
    }
  }
  return res;
}


bool _lookForAPossibleEffect(bool& pSatisfyObjective,
                             std::map<std::string, std::set<std::string>>& pParameters,
                             const WorldModification& pEffectToCheck,
                             const Goal& pGoal,
                             const Problem& pProblem,
                             const FactOptional& pFactOptionalToSatisfy,
                             const Domain& pDomain,
                             FactsAlreadyChecked& pFactsAlreadychecked)
{
  if (!pFactOptionalToSatisfy.isFactNegated)
  {
    if (pEffectToCheck.factsModifications &&
        pEffectToCheck.factsModifications->forAllFactsUntilTrue(
              [&](const Fact& pFact)
        {
          std::map<std::string, std::set<std::string>> newParameters;
          bool res = pFactOptionalToSatisfy.fact.isInFact(pFact, false, newParameters, &pParameters);
          applyNewParams(pParameters, newParameters);
          return res;
        }, pProblem))
    {
      pSatisfyObjective = true;
      return true;
    }
    if (pEffectToCheck.potentialFactsModifications &&
        pEffectToCheck.potentialFactsModifications->forAllFactsUntilTrue(
              [&](const Fact& pFact)
        {
          std::map<std::string, std::set<std::string>> newParameters;
          bool res = pFactOptionalToSatisfy.fact.isInFact(pFact, false, newParameters, &pParameters);
          applyNewParams(pParameters, newParameters);
          return res;
        }, pProblem))
    {
      pSatisfyObjective = true;
      return true;
    }
  }
  else
  {
    if (pEffectToCheck.factsModifications &&
        pEffectToCheck.factsModifications->forAllNotFactsUntilTrue(
              [&](const Fact& pFact)
        {
          std::map<std::string, std::set<std::string>> newParameters;
          bool res = pFactOptionalToSatisfy.fact.isInFact(pFact, false, newParameters, &pParameters);
          applyNewParams(pParameters, newParameters);
          return res;
        }, pProblem))
    {
      pSatisfyObjective = true;
      return true;
    }
    if (pEffectToCheck.potentialFactsModifications &&
        pEffectToCheck.potentialFactsModifications->forAllNotFactsUntilTrue(
              [&](const Fact& pFact)
        {
          std::map<std::string, std::set<std::string>> newParameters;
          bool res = pFactOptionalToSatisfy.fact.isInFact(pFact, false, newParameters, &pParameters);
          applyNewParams(pParameters, newParameters);
          return res;
        }, pProblem))
    {
      pSatisfyObjective = true;
      return true;
    }
  }

  auto& setOfInferences = pProblem.getSetOfInferences();
  auto& preconditionToActions = pDomain.preconditionToActions();
  bool subRes = pEffectToCheck.forAllFactsUntilTrue([&](const cp::Fact& pFact) {
    // Condition only for optimization
    if (pParameters.empty() && pProblem.facts().count(pFact) > 0)
      return false;

    if (pFactsAlreadychecked.factsToAdd.count(pFact) == 0)
    {
      FactsAlreadyChecked subFactsAlreadychecked = pFactsAlreadychecked;
      subFactsAlreadychecked.factsToAdd.insert(pFact);
      PossibleEffect possibleEffect = _lookForAPossibleExistingOrNotFactFromActions(pFact, false, pParameters, preconditionToActions, pGoal,
                                                                           pProblem, pFactOptionalToSatisfy,
                                                                           pDomain, subFactsAlreadychecked);
      if (possibleEffect != PossibleEffect::SATISFIED)
      {
        for (auto& currSetOfInferences : setOfInferences)
        {
          auto& inferences = currSetOfInferences.second->inferences();
          auto& conditionToReachableInferences = currSetOfInferences.second->reachableInferenceLinks().conditionToInferences;
          possibleEffect = _merge(_lookForAPossibleExistingOrNotFactFromInferences(pFact, false, pParameters, conditionToReachableInferences, inferences,
                                                                                   pGoal, pProblem, pFactOptionalToSatisfy,
                                                                                   pDomain, subFactsAlreadychecked), possibleEffect);
          if (possibleEffect == PossibleEffect::SATISFIED)
            break;
          auto& conditionToUnreachableInferences = currSetOfInferences.second->unreachableInferenceLinks().conditionToInferences;
          possibleEffect = _merge(_lookForAPossibleExistingOrNotFactFromInferences(pFact, false, pParameters, conditionToUnreachableInferences, inferences,
                                                                                   pGoal, pProblem, pFactOptionalToSatisfy,
                                                                                   pDomain, subFactsAlreadychecked), possibleEffect);
          if (possibleEffect == PossibleEffect::SATISFIED)
            break;
        }
      }

      if (possibleEffect != PossibleEffect::SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD)
        pFactsAlreadychecked.swap(subFactsAlreadychecked);
      return possibleEffect == PossibleEffect::SATISFIED;
    }
    return false;
  }, pProblem);
  if (subRes)
    return true;

  auto& notPreconditionToActions = pDomain.notPreconditionToActions();
  return pEffectToCheck.forAllNotFactsUntilTrue([&](const cp::Fact& pFact) {
    // Condition only for optimization
    if (pParameters.empty() && pProblem.facts().count(pFact) == 0)
       return false;

    if (pFactsAlreadychecked.factsToRemove.count(pFact) == 0)
    {
      FactsAlreadyChecked subFactsAlreadychecked = pFactsAlreadychecked;
      subFactsAlreadychecked.factsToRemove.insert(pFact);
      PossibleEffect possibleEffect = _lookForAPossibleExistingOrNotFactFromActions(pFact, true, pParameters, notPreconditionToActions, pGoal,
                                                                                    pProblem, pFactOptionalToSatisfy,
                                                                                    pDomain, subFactsAlreadychecked);

      if (possibleEffect != PossibleEffect::SATISFIED)
      {
        for (auto& currSetOfInferences : setOfInferences)
        {
          auto& inferences = currSetOfInferences.second->inferences();
          auto& notConditionToReachableInferences = currSetOfInferences.second->reachableInferenceLinks().notConditionToInferences;
          possibleEffect = _merge(_lookForAPossibleExistingOrNotFactFromInferences(pFact, true, pParameters, notConditionToReachableInferences, inferences,
                                                                                   pGoal, pProblem, pFactOptionalToSatisfy,
                                                                                   pDomain, subFactsAlreadychecked), possibleEffect);
          if (possibleEffect == PossibleEffect::SATISFIED)
            break;
          auto& notConditionToUnreachableInferences = currSetOfInferences.second->unreachableInferenceLinks().notConditionToInferences;
          possibleEffect = _merge(_lookForAPossibleExistingOrNotFactFromInferences(pFact, true, pParameters, notConditionToUnreachableInferences, inferences,
                                                                                   pGoal, pProblem, pFactOptionalToSatisfy,
                                                                                   pDomain, subFactsAlreadychecked), possibleEffect);
          if (possibleEffect == PossibleEffect::SATISFIED)
            break;
        }
      }

      if (possibleEffect != PossibleEffect::SATISFIED_BUT_DOES_NOT_MODIFY_THE_WORLD)
        pFactsAlreadychecked.swap(subFactsAlreadychecked);
      return possibleEffect == PossibleEffect::SATISFIED;
    }
    return false;
  }, pProblem);
}


void _nextStepOfTheProblemForAGoalAndSetOfActions(PotentialNextAction& pCurrentResult,
                                                  const std::set<ActionId>& pActions,
                                                  const Goal& pGoal,
                                                  const Problem& pProblem,
                                                  const FactOptional& pFactOptionalToSatisfy,
                                                  const Domain& pDomain,
                                                  const Historical* pGlobalHistorical)
{
  PotentialNextAction newPotNextAction;
  auto& domainActions = pDomain.actions();
  for (const auto& currAction : pActions)
  {
    auto itAction = domainActions.find(currAction);
    if (itAction != domainActions.end())
    {
      auto& action = itAction->second;
      FactsAlreadyChecked factsAlreadyChecked;
      auto newPotRes = PotentialNextAction(currAction, action);
      if (_lookForAPossibleEffect(newPotRes.satisfyObjective, newPotRes.parameters, action.effect, pGoal,
                                  pProblem, pFactOptionalToSatisfy, pDomain, factsAlreadyChecked) &&
          (!action.precondition || action.precondition->isTrue(pProblem, {}, {}, &newPotRes.parameters)))
      {
        if (newPotRes.isMoreImportantThan(newPotNextAction, pProblem, pGlobalHistorical))
        {
          assert(newPotRes.actionPtr != nullptr);
          newPotNextAction = newPotRes;
        }
      }
    }
  }

  if (newPotNextAction.isMoreImportantThan(pCurrentResult, pProblem, pGlobalHistorical))
  {
    assert(newPotNextAction.actionPtr != nullptr);
    pCurrentResult = newPotNextAction;
  }
}


ActionId _nextStepOfTheProblemForAGoal(
    std::map<std::string, std::set<std::string>>& pParameters,
    const Goal& pGoal,
    const Problem& pProblem,
    const FactOptional& pFactOptionalToSatisfy,
    const Domain& pDomain,
    const Historical* pGlobalHistorical)
{
  PotentialNextAction res;
  for (const auto& currFact : pProblem.factNamesToFacts())
  {
    auto itPrecToActions = pDomain.preconditionToActions().find(currFact.first);
    if (itPrecToActions != pDomain.preconditionToActions().end())
      _nextStepOfTheProblemForAGoalAndSetOfActions(res, itPrecToActions->second, pGoal,
                                                   pProblem, pFactOptionalToSatisfy,
                                                   pDomain, pGlobalHistorical);
  }
  for (const auto& currFact : pProblem.variablesToValue())
  {
    auto itPrecToActions = pDomain.preconditionToActionsExps().find(currFact.first);
    if (itPrecToActions != pDomain.preconditionToActionsExps().end())
      _nextStepOfTheProblemForAGoalAndSetOfActions(res, itPrecToActions->second, pGoal,
                                                   pProblem, pFactOptionalToSatisfy,
                                                   pDomain, pGlobalHistorical);
  }
  auto& actionsWithoutFactToAddInPrecondition = pDomain.actionsWithoutFactToAddInPrecondition();
  _nextStepOfTheProblemForAGoalAndSetOfActions(res, actionsWithoutFactToAddInPrecondition, pGoal,
                                               pProblem, pFactOptionalToSatisfy,
                                               pDomain, pGlobalHistorical);
  pParameters = std::move(res.parameters);
  return res.actionId;
}


}


std::unique_ptr<OneStepOfPlannerResult> lookForAnActionToDo(
    Problem& pProblem,
    const Domain& pDomain,
    const std::unique_ptr<std::chrono::steady_clock::time_point>& pNow,
    const Historical* pGlobalHistorical)
{
  pProblem.fillAccessibleFacts(pDomain);

  std::unique_ptr<OneStepOfPlannerResult> res;
  auto tryToFindAnActionTowardGoal = [&](Goal& pGoal, int pPriority){
    const FactOptional* factOptionalToSatisfyPtr = nullptr;
    pGoal.factCondition().untilFalse(
          [&](const FactOptional& pFactOptional)
    {
      if (!pProblem.isOptionalFactSatisfied(pFactOptional))
      {
        factOptionalToSatisfyPtr = &pFactOptional;
        return false;
      }
      return true;
    }, [](const Expression&) { return true; }, pProblem, {});

    if (factOptionalToSatisfyPtr != nullptr)
    {
      std::map<std::string, std::set<std::string>> parameters;
      auto actionId =
          _nextStepOfTheProblemForAGoal(parameters, pGoal,
                                        pProblem, *factOptionalToSatisfyPtr, pDomain, pGlobalHistorical);
      if (!actionId.empty())
      {
        res = std::make_unique<OneStepOfPlannerResult>(actionId, parameters, pGoal, pPriority);
        pGoal.notifyActivity();
        return true;
      }
      return false;
    }
    return false;
  };

  pProblem.iterateOnGoalsAndRemoveNonPersistent(tryToFindAnActionTowardGoal, pNow);
  return res;
}



void notifyActionDone(Problem& pProblem,
                      const Domain& pDomain,
                      const OneStepOfPlannerResult& pOnStepOfPlannerResult,
                      const std::unique_ptr<std::chrono::steady_clock::time_point>& pNow)
{
  auto itAction = pDomain.actions().find(pOnStepOfPlannerResult.actionInstance.actionId);
  if (itAction != pDomain.actions().end())
    pProblem.notifyActionDone(pOnStepOfPlannerResult, itAction->second.effect.factsModifications, pNow,
                              &itAction->second.effect.goalsToAdd, &itAction->second.effect.goalsToAddInCurrentPriority);
}


std::list<ActionInstance> lookForResolutionPlan(
    Problem& pProblem,
    const Domain& pDomain,
    const std::unique_ptr<std::chrono::steady_clock::time_point>& pNow,
    Historical* pGlobalHistorical)
{
  std::set<std::string> actionAlreadyInPlan;
  std::list<ActionInstance> res;
  while (!pProblem.goals().empty())
  {
    auto onStepOfPlannerResult = lookForAnActionToDo(pProblem, pDomain, pNow, pGlobalHistorical);
    if (!onStepOfPlannerResult)
      break;
    res.emplace_back(onStepOfPlannerResult->actionInstance);
    const auto& actionToDo = onStepOfPlannerResult->actionInstance.actionId;
    if (actionAlreadyInPlan.count(actionToDo) > 0)
      break;
    actionAlreadyInPlan.insert(actionToDo);

    auto itAction = pDomain.actions().find(actionToDo);
    if (itAction != pDomain.actions().end())
    {
      if (pGlobalHistorical != nullptr)
        pGlobalHistorical->notifyActionDone(actionToDo);
      pProblem.notifyActionDone(*onStepOfPlannerResult, itAction->second.effect.factsModifications, pNow,
                                &itAction->second.effect.goalsToAdd, &itAction->second.effect.goalsToAddInCurrentPriority);
    }
  }
  return res;
}


} // !cp
