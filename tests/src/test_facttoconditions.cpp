#include <gtest/gtest.h>
#include <prioritizedgoalsplanner/types/fact.hpp>
#include <prioritizedgoalsplanner/types/factstovalue.hpp>
#include <prioritizedgoalsplanner/types/ontology.hpp>
#include <prioritizedgoalsplanner/util/alias.hpp>

using namespace pgp;



TEST(Tool, test_factToConditions)
{
  pgp::Ontology ontology;
  ontology.types = pgp::SetOfTypes::fromPddl("entity\n"
                                            "my_type my_type2 - entity\n"
                                            "sub_my_type2 - my_type2");
  ontology.constants = pgp::SetOfEntities::fromPddl("toto toto2 - my_type\n"
                                                   "titi titi_const - my_type2", ontology.types);
  ontology.predicates = pgp::SetOfPredicates::fromStr("pred_name(?e - entity)\n"
                                                     "pred_name2(?e - entity)\n"
                                                     "pred_name3(?p1 - my_type, ?p2 - my_type2)\n"
                                                     "pred_name4(?p1 - my_type, ?p2 - my_type2)\n"
                                                     "pred_name5(?p1 - my_type) - my_type2\n"
                                                     "pred_name6(?e - entity)\n",
                                                     ontology.types);

  auto entities = pgp::SetOfEntities::fromPddl("toto3 - my_type\n"
                                              "titi2 - my_type2\n"
                                              "titi3 - sub_my_type2", ontology.types);

  FactsToValue factToActions;

  auto fact1 = pgp::Fact::fromStr("pred_name(toto)", ontology, entities, {});
  factToActions.add(fact1, "action1");

  {
    EXPECT_EQ("[action1]", factToActions.find(fact1).toStr());
  }

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p - my_type", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name(?p)", ontology, entities, parameters);
    EXPECT_EQ("[action1]", factToActions.find(factWithParam).toStr());
  }

  auto fact2 = pgp::Fact::fromStr("pred_name(toto2)", ontology, entities, {});
  factToActions.add(fact2, "action2");

  auto fact2b = pgp::Fact::fromStr("pred_name6(toto)", ontology, entities, {});
  factToActions.add(fact2b, "action2");

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name6(toto)", ontology, entities, {});
    EXPECT_EQ("[action2]", factToActions.find(factWithParam).toStr());
  }

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p - my_type", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name(?p)", ontology, entities, parameters);
    EXPECT_EQ("[action1, action2]", factToActions.find(factWithParam).toStr());
  }

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p - my_type", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name2(?p)", ontology, entities, parameters);
    EXPECT_EQ("[]", factToActions.find(factWithParam).toStr());
  }

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p2 - my_type2", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name3(toto, ?p2)", ontology, entities, parameters);
    EXPECT_EQ("[]", factToActions.find(factWithParam).toStr());
  }

  auto fact3 = pgp::Fact::fromStr("pred_name3(toto, titi)", ontology, entities, {});
  factToActions.add(fact3, "action3");

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p2 - my_type2", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name3(toto, ?p2)", ontology, entities, parameters);
    EXPECT_EQ("[action3]", factToActions.find(factWithParam).toStr());
  }

  std::vector<pgp::Parameter> fact4Parameters(1, pgp::Parameter::fromStr("?p - my_type2", ontology.types));
  auto fact4 = pgp::Fact::fromStr("pred_name3(toto, ?p)", ontology, entities, fact4Parameters);
  factToActions.add(fact4, "action4");

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p2 - my_type2", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name3(toto, ?p2)", ontology, entities, parameters);
    EXPECT_EQ("[action3, action4]", factToActions.find(factWithParam).toStr());
  }

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name3(toto, titi)", ontology, entities, {});
    EXPECT_EQ("[action3, action4]", factToActions.find(factWithParam).toStr());
  }

  std::vector<pgp::Parameter> fact4bParameters(1, pgp::Parameter::fromStr("?p - my_type2", ontology.types));
  auto fact4b = pgp::Fact::fromStr("pred_name3(toto, ?p)", ontology, entities, fact4bParameters);
  factToActions.add(fact4b, "action4b");

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p1 - my_type", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name3(?p1, titi)", ontology, entities, parameters);
    EXPECT_EQ("[action3, action4, action4b]", factToActions.find(factWithParam).toStr());
  }

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p1 - my_type", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name3(?p1, titi2)", ontology, entities, parameters);
    EXPECT_EQ("[action4, action4b]", factToActions.find(factWithParam).toStr());
  }

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name3(toto3, titi2)", ontology, entities, {});
    EXPECT_EQ("[]", factToActions.find(factWithParam).toStr());
  }

  {
    std::vector<pgp::Parameter> parameters(1, pgp::Parameter::fromStr("?p1 - my_type", ontology.types));
    auto factWithParam = pgp::Fact::fromStr("pred_name3(?p1, titi3)", ontology, entities, parameters);
    EXPECT_EQ("[action4, action4b]", factToActions.find(factWithParam).toStr());
  }

  auto fact5 = pgp::Fact::fromStr("pred_name3(toto, titi_const)", ontology, entities, {});
  factToActions.add(fact5, "action5");

  auto fact6 = pgp::Fact::fromStr("pred_name3(toto2, titi)", ontology, entities, {});
  factToActions.add(fact6, "action6");

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name3(toto, titi)", ontology, entities, {});
    EXPECT_EQ("[action3, action4, action4b]", factToActions.find(factWithParam).toStr());
  }

  // tests with pred_name4

  auto fact7 = pgp::Fact::fromStr("pred_name4(toto, titi_const)", ontology, entities, {});
  factToActions.add(fact7, "action7");

  std::vector<pgp::Parameter> fact8Parameters(1, pgp::Parameter::fromStr("?p2 - my_type2", ontology.types));
  auto fact8 = pgp::Fact::fromStr("pred_name4(toto, ?p2)", ontology, entities, fact8Parameters);
  factToActions.add(fact8, "action8");

  std::vector<pgp::Parameter> fact9Parameters(1, pgp::Parameter::fromStr("?p2 - my_type2", ontology.types));
  auto fact9 = pgp::Fact::fromStr("pred_name4(toto2, ?p2)", ontology, entities, fact9Parameters);
  factToActions.add(fact9, "action9");

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name4(toto, titi)", ontology, entities, {});
    EXPECT_EQ("[action8]", factToActions.find(factWithParam).toStr());
  }


  // tests with pred_name5

  auto fact10 = pgp::Fact::fromStr("pred_name5(toto2)=titi", ontology, entities, {});
  factToActions.add(fact10, "action10");

  std::vector<pgp::Parameter> fact11Parameters(1, pgp::Parameter::fromStr("?p1 - my_type", ontology.types));
  auto fact11 = pgp::Fact::fromStr("pred_name5(?p1)=titi", ontology, entities, fact11Parameters);
  factToActions.add(fact11, "action11");

  std::vector<pgp::Parameter> fact12Parameters(1, pgp::Parameter::fromStr("?p1 - my_type", ontology.types));
  auto fact12 = pgp::Fact::fromStr("pred_name5(?p1)=titi_const", ontology, entities, fact12Parameters);
  factToActions.add(fact12, "action12");

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name5(toto)=titi", ontology, entities, {});
    EXPECT_EQ("[action11]", factToActions.find(factWithParam).toStr());
    EXPECT_EQ("[action11, action12]", factToActions.find(factWithParam, true).toStr());
  }

  auto fact13 = pgp::Fact::fromStr("pred_name5(toto2)!=titi", ontology, entities, {});
  factToActions.add(fact13, "action13");

  {
    auto factWithParam = pgp::Fact::fromStr("pred_name5(toto)=titi_const", ontology, entities, {});
    EXPECT_EQ("[action12, action13]", factToActions.find(factWithParam).toStr());
  }

  /*
  {
    auto factWithParam = pgp::Fact::fromStr("pred_name5(toto)!=titi_const", ontology, entities, {});
    EXPECT_EQ("[action11]", factToActions.find(factWithParam).toStr());
    EXPECT_EQ("[action11, action12]", factToActions.find(factWithParam, true).toStr());
  }
  */
}
