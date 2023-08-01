#include <contextualplanner/types/factcondition.hpp>
#include <contextualplanner/types/problem.hpp>

namespace cp
{
namespace
{
enum class ExpressionOperator
{
  PLUSPLUS,
  PLUS,
  MINUS,
  EQUAL,
  NOT
};

const std::map<std::string, ExpressionOperator> _strToBeginOfTextOperators
{{"++", ExpressionOperator::PLUSPLUS}};
const std::map<char, ExpressionOperator> _charToOperators
{{'=', ExpressionOperator::EQUAL}, {'+', ExpressionOperator::PLUS}, {'+', ExpressionOperator::MINUS}};



std::unique_ptr<FactCondition> _merge(std::list<std::unique_ptr<FactCondition>>& pFactconditions)
{
  if (pFactconditions.empty())
    return {};
  if (pFactconditions.size() == 1)
    return std::move(pFactconditions.front());

  auto frontElt = std::move(pFactconditions.front());
  pFactconditions.pop_front();
  return std::make_unique<FactConditionNode>(FactConditionNodeType::AND,
                                             std::move(frontElt),
                                             _merge(pFactconditions));
}

}


FactCondition::FactCondition(FactConditionType pType)
 : type(pType)
{
}


std::unique_ptr<FactCondition> FactCondition::fromStr(const std::string& pStr)
{
  std::vector<FactOptional> vect;
  Fact::splitFactOptional(vect, pStr, '&');
  std::list<std::unique_ptr<FactCondition>> factconditions;

  for (auto& currOptFact : vect)
  {
    if (currOptFact.fact.name.empty())
      continue;
    std::string currentToken;
    Expression exp;
    auto fillCurrentToken = [&]
    {
      if (!currentToken.empty())
      {
        exp.elts.emplace_back(ExpressionElementType::VALUE, currentToken);
        currentToken.clear();
      }
    };

    if (!currOptFact.fact.parameters.empty() ||
        currOptFact.fact.name[0] != '$')
    {
      if (currOptFact.fact.name == "equals" &&
          currOptFact.fact.parameters.size() == 2 &&
          currOptFact.fact.value.empty())
      {
        factconditions.emplace_back(std::make_unique<FactConditionNode>(
                                      FactConditionNodeType::EQUALITY,
                                      std::make_unique<FactConditionFact>(currOptFact.fact.parameters[0]),
                                    std::make_unique<FactConditionFact>(currOptFact.fact.parameters[1])));
      }
      else
      {
        factconditions.emplace_back(std::make_unique<FactConditionFact>(currOptFact));
      }
      continue;
    }

    auto currStr = currOptFact.fact.toStr();
    for (std::size_t i = 0; i < currStr.size();)
    {
      bool needToContinue = false;
      if (i == 0)
      {
        for (const auto& currOp : _strToBeginOfTextOperators)
        {
          if (currStr.compare(0, currOp.first.size(), currOp.first) == 0)
          {
            fillCurrentToken();
            exp.elts.emplace_back(ExpressionElementType::OPERATOR, currOp.first);
            i += currOp.first.size();
            needToContinue = true;
            break;
          }
        }
      }
      if (needToContinue)
        continue;
      for (const auto& charToOp : _charToOperators)
      {
        if (charToOp.first == currStr[i])
        {
          fillCurrentToken();
          exp.elts.emplace_back(ExpressionElementType::OPERATOR, std::string(1, charToOp.first));
          ++i;
          needToContinue = true;
          break;
        }
      }
      if (needToContinue)
        continue;
      if (currStr[i] == '$' && currStr[i+1] == '{')
      {
        auto endOfVar = currStr.find('}', i + 2);
        if (endOfVar != std::string::npos)
        {
          fillCurrentToken();
          auto begPos = i + 2;
          exp.elts.emplace_back(ExpressionElementType::FACT, currStr.substr(begPos, endOfVar - begPos));
          i = endOfVar + 1;
          continue;
        }
      }
      currentToken += currStr[i++];
    }
    fillCurrentToken();
    factconditions.emplace_back(std::make_unique<FactConditionExpression>(exp));
  }

  return _merge(factconditions);
}


FactConditionNode::FactConditionNode(FactConditionNodeType pNodeType,
                                     std::unique_ptr<FactCondition> pLeftOperand,
                                     std::unique_ptr<FactCondition> pRightOperand)
 : FactCondition(FactConditionType::NODE),
   nodeType(pNodeType),
   leftOperand(std::move(pLeftOperand)),
   rightOperand(std::move(pRightOperand))
{
}

bool FactConditionNode::hasFact(const Fact& pFact) const
{
  return (leftOperand && leftOperand->hasFact(pFact)) ||
      (rightOperand && rightOperand->hasFact(pFact));
}


bool FactConditionNode::containsFact(const Fact& pFact) const
{
  return (leftOperand && leftOperand->containsFact(pFact)) ||
      (rightOperand && rightOperand->containsFact(pFact));
}

bool FactConditionNode::containsNotFact(const Fact& pFact) const
{
  return (leftOperand && leftOperand->containsNotFact(pFact)) ||
      (rightOperand && rightOperand->containsNotFact(pFact));
}

bool FactConditionNode::containsExpression(const Expression& pExpression) const
{
  return (leftOperand && leftOperand->containsExpression(pExpression)) ||
      (rightOperand && rightOperand->containsExpression(pExpression));
}


void FactConditionNode::replaceFact(const cp::Fact& pOldFact,
                                    const Fact& pNewFact)
{
  if (leftOperand)
    leftOperand->replaceFact(pOldFact, pNewFact);
  if (rightOperand)
    rightOperand->replaceFact(pOldFact, pNewFact);
}


void FactConditionNode::forAll(const std::function<void (const FactOptional&)>& pFactCallback,
                               const std::function<void (const Expression&)>& pExpCallback) const
{
  if (leftOperand)
    leftOperand->forAll(pFactCallback, pExpCallback);
  if (rightOperand)
    rightOperand->forAll(pFactCallback, pExpCallback);
}

bool FactConditionNode::untilFalse(const std::function<bool (const FactOptional&)>& pFactCallback,
                                   const std::function<bool (const Expression&)>& pExpCallback,
                                   const Problem& pProblem) const
{
  if (nodeType == FactConditionNodeType::AND)
  {
    if (leftOperand && !leftOperand->untilFalse(pFactCallback, pExpCallback, pProblem))
      return false;
    if (rightOperand && !rightOperand->untilFalse(pFactCallback, pExpCallback, pProblem))
      return false;
  }
  else if (nodeType == FactConditionNodeType::EQUALITY && leftOperand && rightOperand)
  {
    auto* leftFactPtr = leftOperand->fcFactPtr();
    auto* rightFactPtr = rightOperand->fcFactPtr();
    if (leftFactPtr != nullptr && rightFactPtr != nullptr)
    {
      auto factToCheck = leftFactPtr->factOptional.fact;
      factToCheck.value = pProblem.getFactValue(rightFactPtr->factOptional.fact);
      return pFactCallback(FactOptional(factToCheck));
    }
  }
  return true;
}

bool FactConditionNode::canBeTrue() const
{
  if (leftOperand && !leftOperand->canBeTrue())
    return false;
  if (rightOperand && !rightOperand->canBeTrue())
    return false;
  return true;
}

bool FactConditionNode::isTrue(const Problem& pProblem,
                               const std::set<Fact>& pPunctualFacts,
                               std::map<std::string, std::string>* pParametersPtr) const
{
  if (nodeType == FactConditionNodeType::AND)
  {
    if (leftOperand && !leftOperand->isTrue(pProblem, pPunctualFacts, pParametersPtr))
      return false;
    if (rightOperand && !rightOperand->isTrue(pProblem, pPunctualFacts, pParametersPtr))
      return false;
  }
  else if (nodeType == FactConditionNodeType::EQUALITY && leftOperand && rightOperand)
  {
    auto* leftFactPtr = leftOperand->fcFactPtr();
    auto* rightFactPtr = rightOperand->fcFactPtr();
    if (leftFactPtr != nullptr && rightFactPtr != nullptr)
    {
      auto factToCheck = leftFactPtr->factOptional.fact;
      factToCheck.value = pProblem.getFactValue(rightFactPtr->factOptional.fact);
      if (factToCheck.isPunctual())
        return pPunctualFacts.count(factToCheck) != 0;
      return factToCheck.isInFacts(pProblem._facts, true, pParametersPtr);
    }
  }
  return true;
}

bool FactConditionNode::canBecomeTrue(const Problem& pProblem) const
{
  if (nodeType == FactConditionNodeType::AND)
  {
    if (leftOperand && !leftOperand->canBecomeTrue(pProblem))
      return false;
    if (rightOperand && !rightOperand->canBecomeTrue(pProblem))
      return false;
  }
  else if (nodeType == FactConditionNodeType::EQUALITY && leftOperand && rightOperand)
  {
    auto* leftFactPtr = leftOperand->fcFactPtr();
    auto* rightFactPtr = rightOperand->fcFactPtr();
    if (leftFactPtr != nullptr && rightFactPtr != nullptr)
    {
      auto factToCheck = leftFactPtr->factOptional.fact;
      factToCheck.value = pProblem.getFactValue(rightFactPtr->factOptional.fact);
      return pProblem.canFactBecomeTrue(factToCheck);
    }
  }
  return true;
}



std::unique_ptr<FactCondition> FactConditionNode::clone() const
{
  return std::make_unique<FactConditionNode>(
        nodeType,
        leftOperand ? leftOperand->clone() : std::unique_ptr<FactCondition>(),
        rightOperand ? rightOperand->clone() : std::unique_ptr<FactCondition>());
}


FactConditionFact::FactConditionFact(const FactOptional& pFactOptional)
 : FactCondition(FactConditionType::FACT),
   factOptional(pFactOptional)
{
}

bool FactConditionFact::hasFact(const cp::Fact& pFact) const
{
  return factOptional.fact == pFact;
}

bool FactConditionFact::containsFact(const Fact& pFact) const
{
  return !factOptional.isFactNegated && factOptional.fact == pFact;
}

bool FactConditionFact::containsNotFact(const Fact& pFact) const
{
  return factOptional.isFactNegated && factOptional.fact == pFact;
}

void FactConditionFact::replaceFact(const cp::Fact& pOldFact,
                                    const Fact& pNewFact)
{
  if (factOptional.fact == pOldFact)
    factOptional.fact = pNewFact;
}

bool FactConditionFact::isTrue(const Problem& pProblem,
                               const std::set<Fact>& pPunctualFacts,
                               std::map<std::string, std::string>* pParametersPtr) const
{
  if (factOptional.isFactNegated)
    return !factOptional.fact.isInFacts(pProblem._facts, true, pParametersPtr);

  if (factOptional.fact.isPunctual())
    return pPunctualFacts.count(factOptional.fact) != 0;

  return factOptional.fact.isInFacts(pProblem._facts, true, pParametersPtr);
}

bool FactConditionFact::canBecomeTrue(const Problem& pProblem) const
{
  if (factOptional.isFactNegated)
  {
    if (pProblem._facts.count(factOptional.fact) > 0 &&
        pProblem._removableFacts.count(factOptional.fact) == 0)
      return false;
    return true;
  }
  return pProblem.canFactBecomeTrue(factOptional.fact);
}

std::unique_ptr<FactCondition> FactConditionFact::clone() const
{
  return std::make_unique<FactConditionFact>(factOptional);
}



FactConditionExpression::FactConditionExpression(const Expression& pExpression)
 : FactCondition(FactConditionType::EXPRESSION),
   expression(pExpression)
{
}

bool FactConditionExpression::hasFact(const cp::Fact& pFact) const
{
  for (auto& currElt : expression.elts)
    if (currElt.type == ExpressionElementType::FACT && currElt.value == pFact.toStr())
      return true;
  return false;
}

bool FactConditionExpression::containsExpression(const Expression& pExpression) const
{
  return expression == pExpression;
}

void FactConditionExpression::replaceFact(const cp::Fact& pOldFact,
                                          const Fact& pNewFact)
{
  for (auto& currElt : expression.elts)
    if (currElt.type == ExpressionElementType::FACT && currElt.value == pOldFact.toStr())
      currElt.value = pNewFact.toStr();
}

bool FactConditionExpression::isTrue(const Problem& pProblem,
                                     const std::set<Fact>& pPunctualFacts,
                                     std::map<std::string, std::string>* pParametersPtr) const
{
  return expression.isValid(pProblem.variablesToValue());
}

bool FactConditionExpression::canBecomeTrue(const Problem& pProblem) const
{
  return expression.isValid(pProblem.variablesToValue());
}


std::unique_ptr<FactCondition> FactConditionExpression::clone() const
{
  return std::make_unique<FactConditionExpression>(expression);
}


} // !cp