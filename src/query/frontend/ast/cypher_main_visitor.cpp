#include "query/frontend/ast/cypher_main_visitor.hpp"

#include <algorithm>
#include <climits>
#include <codecvt>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "database/graph_db.hpp"
#include "query/exceptions.hpp"
#include "utils/assert.hpp"

namespace query {
namespace frontend {

const std::string CypherMainVisitor::kAnonPrefix = "anon";

antlrcpp::Any CypherMainVisitor::visitSingleQuery(
    CypherParser::SingleQueryContext *ctx) {
  query_ = storage_.query();
  for (auto *child : ctx->clause()) {
    antlrcpp::Any got = child->accept(this);
    if (got.is<Clause *>()) {
      query_->clauses_.push_back(got.as<Clause *>());
    } else {
      auto child_clauses = got.as<std::vector<Clause *>>();
      query_->clauses_.insert(query_->clauses_.end(), child_clauses.begin(),
                              child_clauses.end());
    }
  }
  // Construct unique names for anonymous identifiers;
  int id = 1;
  for (auto **identifier : anonymous_identifiers) {
    while (true) {
      std::string id_name = kAnonPrefix + std::to_string(id++);
      if (users_identifiers.find(id_name) == users_identifiers.end()) {
        *identifier = storage_.Create<Identifier>(id_name);
        break;
      }
    }
  }
  return query_;
}

antlrcpp::Any CypherMainVisitor::visitClause(CypherParser::ClauseContext *ctx) {
  if (ctx->cypherReturn()) {
    return static_cast<Clause *>(
        ctx->cypherReturn()->accept(this).as<Return *>());
  }
  if (ctx->cypherMatch()) {
    return static_cast<Clause *>(
        ctx->cypherMatch()->accept(this).as<Match *>());
  }
  if (ctx->create()) {
    return static_cast<Clause *>(ctx->create()->accept(this).as<Create *>());
  }
  if (ctx->cypherDelete()) {
    return static_cast<Clause *>(
        ctx->cypherDelete()->accept(this).as<Delete *>());
  }
  if (ctx->set()) {
    // Different return type!!!
    return ctx->set()->accept(this).as<std::vector<Clause *>>();
  }
  // TODO: implement other clauses.
  throw NotYetImplemented();
  return 0;
}

antlrcpp::Any CypherMainVisitor::visitCypherMatch(
    CypherParser::CypherMatchContext *ctx) {
  auto *match = storage_.Create<Match>();
  if (ctx->OPTIONAL()) {
    // TODO: implement other clauses.
    throw NotYetImplemented();
  }
  if (ctx->where()) {
    match->where_ = ctx->where()->accept(this);
  }
  match->patterns_ = ctx->pattern()->accept(this).as<std::vector<Pattern *>>();
  return match;
}

antlrcpp::Any CypherMainVisitor::visitCreate(CypherParser::CreateContext *ctx) {
  auto *create = storage_.Create<Create>();
  create->patterns_ = ctx->pattern()->accept(this).as<std::vector<Pattern *>>();
  return create;
  ;
}

antlrcpp::Any CypherMainVisitor::visitCypherReturn(
    CypherParser::CypherReturnContext *ctx) {
  if (ctx->DISTINCT()) {
    // TODO: implement other clauses.
    throw NotYetImplemented();
  }
  return visitChildren(ctx);
}

antlrcpp::Any CypherMainVisitor::visitReturnBody(
    CypherParser::ReturnBodyContext *ctx) {
  if (ctx->order() || ctx->skip() || ctx->limit()) {
    // TODO: implement other clauses.
    throw NotYetImplemented();
  }
  return ctx->returnItems()->accept(this);
}

antlrcpp::Any CypherMainVisitor::visitReturnItems(
    CypherParser::ReturnItemsContext *ctx) {
  auto *return_clause = storage_.Create<Return>();
  if (ctx->getTokens(kReturnAllTokenId).size()) {
    // TODO: implement other clauses.
    throw NotYetImplemented();
  }
  for (auto *item : ctx->returnItem()) {
    return_clause->named_expressions_.push_back(item->accept(this));
  }
  return return_clause;
}

antlrcpp::Any CypherMainVisitor::visitReturnItem(
    CypherParser::ReturnItemContext *ctx) {
  auto *named_expr = storage_.Create<NamedExpression>();
  if (ctx->variable()) {
    named_expr->name_ =
        std::string(ctx->variable()->accept(this).as<std::string>());
  } else {
    // TODO: Should we get this by text or some escaping is needed?
    named_expr->name_ = std::string(ctx->getText());
  }
  named_expr->expression_ = ctx->expression()->accept(this);
  return named_expr;
}

antlrcpp::Any CypherMainVisitor::visitNodePattern(
    CypherParser::NodePatternContext *ctx) {
  auto *node = storage_.Create<NodeAtom>();
  if (ctx->variable()) {
    std::string variable = ctx->variable()->accept(this);
    node->identifier_ = storage_.Create<Identifier>(variable);
    users_identifiers.insert(variable);
  } else {
    anonymous_identifiers.push_back(&node->identifier_);
  }
  if (ctx->nodeLabels()) {
    node->labels_ =
        ctx->nodeLabels()->accept(this).as<std::vector<GraphDb::Label>>();
  }
  if (ctx->properties()) {
    node->properties_ = ctx->properties()
                            ->accept(this)
                            .as<std::map<GraphDb::Property, Expression *>>();
  }
  return node;
}

antlrcpp::Any CypherMainVisitor::visitNodeLabels(
    CypherParser::NodeLabelsContext *ctx) {
  std::vector<GraphDb::Label> labels;
  for (auto *node_label : ctx->nodeLabel()) {
    labels.push_back(ctx_.db_accessor_.label(node_label->accept(this)));
  }
  return labels;
}

antlrcpp::Any CypherMainVisitor::visitProperties(
    CypherParser::PropertiesContext *ctx) {
  if (!ctx->mapLiteral()) {
    // If child is not mapLiteral that means child is params. At the moment
    // we don't support properties to be a param because we can generate
    // better logical plan if we have an information about properties at
    // compile time.
    // TODO: implement other clauses.
    throw NotYetImplemented();
  }
  return ctx->mapLiteral()->accept(this);
}

antlrcpp::Any CypherMainVisitor::visitMapLiteral(
    CypherParser::MapLiteralContext *ctx) {
  std::map<GraphDb::Property, Expression *> map;
  for (int i = 0; i < (int)ctx->propertyKeyName().size(); ++i) {
    map[ctx->propertyKeyName()[i]->accept(this)] =
        ctx->expression()[i]->accept(this);
  }
  return map;
}

antlrcpp::Any CypherMainVisitor::visitPropertyKeyName(
    CypherParser::PropertyKeyNameContext *ctx) {
  return ctx_.db_accessor_.property(visitChildren(ctx));
}

antlrcpp::Any CypherMainVisitor::visitSymbolicName(
    CypherParser::SymbolicNameContext *ctx) {
  if (ctx->EscapedSymbolicName()) {
    // We don't allow at this point for variable to be EscapedSymbolicName
    // because we would have t ofigure out how escaping works since same
    // variable can be referenced in two ways: escaped and unescaped.
    // TODO: implement other clauses.
    throw NotYetImplemented();
  }
  // TODO: We should probably escape string.
  return std::string(ctx->getText());
}

antlrcpp::Any CypherMainVisitor::visitPattern(
    CypherParser::PatternContext *ctx) {
  std::vector<Pattern *> patterns;
  for (auto *pattern_part : ctx->patternPart()) {
    patterns.push_back(pattern_part->accept(this));
  }
  return patterns;
}

antlrcpp::Any CypherMainVisitor::visitPatternPart(
    CypherParser::PatternPartContext *ctx) {
  Pattern *pattern = ctx->anonymousPatternPart()->accept(this);
  if (ctx->variable()) {
    std::string variable = ctx->variable()->accept(this);
    pattern->identifier_ = storage_.Create<Identifier>(variable);
    users_identifiers.insert(variable);
  } else {
    anonymous_identifiers.push_back(&pattern->identifier_);
  }
  return pattern;
}

antlrcpp::Any CypherMainVisitor::visitPatternElement(
    CypherParser::PatternElementContext *ctx) {
  if (ctx->patternElement()) {
    return ctx->patternElement()->accept(this);
  }
  auto pattern = storage_.Create<Pattern>();
  pattern->atoms_.push_back(ctx->nodePattern()->accept(this).as<NodeAtom *>());
  for (auto *pattern_element_chain : ctx->patternElementChain()) {
    std::pair<PatternAtom *, PatternAtom *> element =
        pattern_element_chain->accept(this);
    pattern->atoms_.push_back(element.first);
    pattern->atoms_.push_back(element.second);
  }
  return pattern;
}

antlrcpp::Any CypherMainVisitor::visitPatternElementChain(
    CypherParser::PatternElementChainContext *ctx) {
  return std::pair<PatternAtom *, PatternAtom *>(
      ctx->relationshipPattern()->accept(this).as<EdgeAtom *>(),
      ctx->nodePattern()->accept(this).as<NodeAtom *>());
}

antlrcpp::Any CypherMainVisitor::visitRelationshipPattern(
    CypherParser::RelationshipPatternContext *ctx) {
  auto *edge = storage_.Create<EdgeAtom>();
  if (ctx->relationshipDetail()) {
    if (ctx->relationshipDetail()->variable()) {
      std::string variable =
          ctx->relationshipDetail()->variable()->accept(this);
      edge->identifier_ = storage_.Create<Identifier>(variable);
      users_identifiers.insert(variable);
    }
    if (ctx->relationshipDetail()->relationshipTypes()) {
      edge->edge_types_ = ctx->relationshipDetail()
                              ->relationshipTypes()
                              ->accept(this)
                              .as<std::vector<GraphDb::EdgeType>>();
    }
    if (ctx->relationshipDetail()->properties()) {
      edge->properties_ = ctx->relationshipDetail()
                              ->properties()
                              ->accept(this)
                              .as<std::map<GraphDb::Property, Expression *>>();
    }
    if (ctx->relationshipDetail()->rangeLiteral()) {
      // TODO: implement other clauses.
      throw NotYetImplemented();
    }
  }
  //    relationship.has_range = true;
  //    auto range = ctx->relationshipDetail()
  //                     ->rangeLiteral()
  //                     ->accept(this)
  //                     .as<std::pair<int64_t, int64_t>>();
  //    relationship.lower_bound = range.first;
  //    relationship.upper_bound = range.second;
  if (!edge->identifier_) {
    anonymous_identifiers.push_back(&edge->identifier_);
  }

  if (ctx->leftArrowHead() && !ctx->rightArrowHead()) {
    edge->direction_ = EdgeAtom::Direction::LEFT;
  } else if (!ctx->leftArrowHead() && ctx->rightArrowHead()) {
    edge->direction_ = EdgeAtom::Direction::RIGHT;
  } else {
    // <-[]-> and -[]- is the same thing as far as we understand openCypher
    // grammar.
    edge->direction_ = EdgeAtom::Direction::BOTH;
  }
  return edge;
}

antlrcpp::Any CypherMainVisitor::visitRelationshipDetail(
    CypherParser::RelationshipDetailContext *) {
  debug_assert(false, "Should never be called. See documentation in hpp.");
  return 0;
}

antlrcpp::Any CypherMainVisitor::visitRelationshipTypes(
    CypherParser::RelationshipTypesContext *ctx) {
  std::vector<GraphDb::EdgeType> types;
  for (auto *edge_type : ctx->relTypeName()) {
    types.push_back(ctx_.db_accessor_.edge_type(edge_type->accept(this)));
  }
  return types;
}

antlrcpp::Any CypherMainVisitor::visitRangeLiteral(
    CypherParser::RangeLiteralContext *ctx) {
  if (ctx->integerLiteral().size() == 0U) {
    // -[*]-
    return std::pair<int64_t, int64_t>(1LL, LLONG_MAX);
  } else if (ctx->integerLiteral().size() == 1U) {
    auto dots_tokens = ctx->getTokens(kDotsTokenId);
    int64_t bound = ctx->integerLiteral()[0]->accept(this).as<int64_t>();
    if (!dots_tokens.size()) {
      // -[*2]-
      return std::pair<int64_t, int64_t>(bound, bound);
    }
    if (dots_tokens[0]->getSourceInterval().startsAfter(
            ctx->integerLiteral()[0]->getSourceInterval())) {
      // -[*2..]-
      return std::pair<int64_t, int64_t>(bound, LLONG_MAX);
    } else {
      // -[*..2]-
      return std::pair<int64_t, int64_t>(1LL, bound);
    }
  } else {
    int64_t lbound = ctx->integerLiteral()[0]->accept(this).as<int64_t>();
    int64_t rbound = ctx->integerLiteral()[1]->accept(this).as<int64_t>();
    // -[*2..5]-
    return std::pair<int64_t, int64_t>(lbound, rbound);
  }
}

antlrcpp::Any CypherMainVisitor::visitExpression(
    CypherParser::ExpressionContext *ctx) {
  return visitChildren(ctx);
}

// OR.
antlrcpp::Any CypherMainVisitor::visitExpression12(
    CypherParser::Expression12Context *ctx) {
  return LeftAssociativeOperatorExpression(ctx->expression11(), ctx->children,
                                           {CypherParser::OR});
}

// XOR.
antlrcpp::Any CypherMainVisitor::visitExpression11(
    CypherParser::Expression11Context *ctx) {
  return LeftAssociativeOperatorExpression(ctx->expression10(), ctx->children,
                                           {CypherParser::XOR});
}

// AND.
antlrcpp::Any CypherMainVisitor::visitExpression10(
    CypherParser::Expression10Context *ctx) {
  return LeftAssociativeOperatorExpression(ctx->expression9(), ctx->children,
                                           {CypherParser::AND});
}

// NOT.
antlrcpp::Any CypherMainVisitor::visitExpression9(
    CypherParser::Expression9Context *ctx) {
  return PrefixUnaryOperator(ctx->expression8(), ctx->children,
                             {CypherParser::NOT});
}

// Comparisons.
// Expresion 1 < 2 < 3 is converted to 1 < 2 && 2 < 3 and then binary operator
// ast node is constructed for each operator.
antlrcpp::Any CypherMainVisitor::visitExpression8(
    CypherParser::Expression8Context *ctx) {
  if (!ctx->partialComparisonExpression().size()) {
    // There is no comparison operators. We generate expression7.
    return ctx->expression7()->accept(this);
  }

  // There is at least one comparison. We need to generate code for each of
  // them. We don't call visitPartialComparisonExpression but do everything in
  // this function and call expression7-s directly. Since every expression7
  // can be generated twice (because it can appear in two comparisons) code
  // generated by whole subtree of expression7 must not have any sideeffects.
  // We handle chained comparisons as defined by mathematics, neo4j handles
  // them in a very interesting, illogical and incomprehensible way. For
  // example in neo4j:
  //  1 < 2 < 3 -> true,
  //  1 < 2 < 3 < 4 -> false,
  //  5 > 3 < 5 > 3 -> true,
  //  4 <= 5 < 7 > 6 -> false
  //  All of those comparisons evaluate to true in memgraph.
  std::vector<Expression *> children;
  children.push_back(ctx->expression7()->accept(this));
  std::vector<size_t> operators;
  auto partial_comparison_expressions = ctx->partialComparisonExpression();
  for (auto *child : partial_comparison_expressions) {
    children.push_back(child->expression7()->accept(this));
  }
  // First production is comparison operator.
  for (auto *child : partial_comparison_expressions) {
    operators.push_back(
        dynamic_cast<antlr4::tree::TerminalNode *>(child->children[0])
            ->getSymbol()
            ->getType());
  }

  // Make all comparisons.
  Expression *first_operand = children[0];
  std::vector<Expression *> comparisons;
  for (int i = 0; i < (int)operators.size(); ++i) {
    auto *expr = children[i + 1];
    // TODO: first_operand should only do lookup if it is only calculated and
    // not recalculated whole subexpression once again. SymbolGenerator should
    // generate symbol for every expresion and then lookup would be possible.
    comparisons.push_back(
        CreateBinaryOperatorByToken(operators[i], first_operand, expr));
    first_operand = expr;
  }

  first_operand = comparisons[0];
  // Calculate logical and of results of comparisons.
  for (int i = 1; i < (int)comparisons.size(); ++i) {
    first_operand = storage_.Create<AndOperator>(first_operand, comparisons[i]);
  }
  return first_operand;
}

antlrcpp::Any CypherMainVisitor::visitPartialComparisonExpression(
    CypherParser::PartialComparisonExpressionContext *) {
  debug_assert(false, "Should never be called. See documentation in hpp.");
  return 0;
}

// Addition and subtraction.
antlrcpp::Any CypherMainVisitor::visitExpression7(
    CypherParser::Expression7Context *ctx) {
  return LeftAssociativeOperatorExpression(ctx->expression6(), ctx->children,
                                           {kPlusTokenId, kMinusTokenId});
}

// Multiplication, division, modding.
antlrcpp::Any CypherMainVisitor::visitExpression6(
    CypherParser::Expression6Context *ctx) {
  return LeftAssociativeOperatorExpression(
      ctx->expression5(), ctx->children,
      {kMultTokenId, kDivTokenId, kModTokenId});
}

// Power.
antlrcpp::Any CypherMainVisitor::visitExpression5(
    CypherParser::Expression5Context *ctx) {
  if (ctx->expression4().size() > 1u) {
    // TODO: implement power operator. In neo4j power is left associative and
    // int^int -> float.
    throw NotYetImplemented();
  }
  return visitChildren(ctx);
}

// Unary minus and plus.
antlrcpp::Any CypherMainVisitor::visitExpression4(
    CypherParser::Expression4Context *ctx) {
  return PrefixUnaryOperator(ctx->expression3(), ctx->children,
                             {kUnaryPlusTokenId, kUnaryMinusTokenId});
}

// List indexing, list range...
antlrcpp::Any CypherMainVisitor::visitExpression3(
    CypherParser::Expression3Context *ctx) {
  // If there is only one child we don't need to generate any code in this since
  // that child is expression2. Other operations are not implemented at the
  // moment.
  // TODO: implement this.
  if (ctx->children.size() > 1u) {
    throw NotYetImplemented();
  }
  return visitChildren(ctx);
}

antlrcpp::Any CypherMainVisitor::visitExpression2(
    CypherParser::Expression2Context *ctx) {
  if (ctx->nodeLabels().size()) {
    // TODO: Implement this. We don't currently support label checking in
    // expresssion.
    throw NotYetImplemented();
  }
  Expression *expression = ctx->atom()->accept(this);
  for (auto *lookup : ctx->propertyLookup()) {
    auto property_lookup =
        storage_.Create<PropertyLookup>(expression, lookup->accept(this));
    expression = property_lookup;
  }
  return expression;
}

antlrcpp::Any CypherMainVisitor::visitAtom(CypherParser::AtomContext *ctx) {
  if (ctx->literal()) {
    return static_cast<Expression *>(visitChildren(ctx).as<Literal *>());
  } else if (ctx->parameter()) {
    // TODO: implement other clauses.
    throw NotYetImplemented();
  } else if (ctx->parenthesizedExpression()) {
    return ctx->parenthesizedExpression()->accept(this);
  } else if (ctx->variable()) {
    std::string variable = ctx->variable()->accept(this);
    users_identifiers.insert(variable);
    return (Expression *)storage_.Create<Identifier>(variable);
  }
  // TODO: Implement this. We don't support comprehensions, functions,
  // filtering... at the moment.
  throw NotYetImplemented();
}

antlrcpp::Any CypherMainVisitor::visitLiteral(
    CypherParser::LiteralContext *ctx) {
  if (ctx->CYPHERNULL()) {
    return storage_.Create<Literal>(TypedValue::Null);
  } else if (ctx->StringLiteral()) {
    return storage_.Create<Literal>(
        visitStringLiteral(ctx->StringLiteral()->getText()).as<std::string>());
  } else if (ctx->booleanLiteral()) {
    return storage_.Create<Literal>(
        ctx->booleanLiteral()->accept(this).as<bool>());
  } else if (ctx->numberLiteral()) {
    return storage_.Create<Literal>(
        ctx->numberLiteral()->accept(this).as<TypedValue>());
  } else {
    // TODO: Implement map and list literals.
    throw NotYetImplemented();
  }
  return visitChildren(ctx);
}

antlrcpp::Any CypherMainVisitor::visitNumberLiteral(
    CypherParser::NumberLiteralContext *ctx) {
  if (ctx->integerLiteral()) {
    return TypedValue(ctx->integerLiteral()->accept(this).as<int64_t>());
  } else if (ctx->doubleLiteral()) {
    return TypedValue(ctx->doubleLiteral()->accept(this).as<double>());
  } else {
    // This should never happen, except grammar changes and we don't notice
    // change in this production.
    debug_assert(false, "can't happen");
    throw std::exception();
  }
}

antlrcpp::Any CypherMainVisitor::visitDoubleLiteral(
    CypherParser::DoubleLiteralContext *ctx) {
  // stod would be nicer but it uses current locale so we shouldn't use it.
  double t = 0LL;
  std::istringstream iss(ctx->getText());
  iss.imbue(std::locale::classic());
  iss >> t;
  if (!iss.eof()) {
    throw SemanticException();
  }
  return t;
}

antlrcpp::Any CypherMainVisitor::visitIntegerLiteral(
    CypherParser::IntegerLiteralContext *ctx) {
  int64_t t = 0LL;
  try {
    // Not really correct since long long can have a bigger range than int64_t.
    t = std::stoll(ctx->getText(), 0, 0);
  } catch (std::out_of_range) {
    throw SemanticException();
  }
  return t;
}

antlrcpp::Any CypherMainVisitor::visitStringLiteral(
    const std::string &escaped) {
  // This function is declared as lambda since its semantics is highly specific
  // for this conxtext and shouldn't be used elsewhere.
  auto EncodeEscapedUnicodeCodepoint = [](const std::string &s, int &i) {
    int j = i + 1;
    const int kShortUnicodeLength = 4;
    const int kLongUnicodeLength = 8;
    while (j < (int)s.size() - 1 && j < i + kLongUnicodeLength + 1 &&
           isxdigit(s[j])) {
      ++j;
    }
    if (j - i == kLongUnicodeLength + 1) {
      char32_t t = stoi(s.substr(i + 1, kLongUnicodeLength), 0, 16);
      i += kLongUnicodeLength;
      std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
      return converter.to_bytes(t);
    } else if (j - i >= kShortUnicodeLength + 1) {
      char16_t t = stoi(s.substr(i + 1, kShortUnicodeLength), 0, 16);
      i += kShortUnicodeLength;
      std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
          converter;
      return converter.to_bytes(t);
    } else {
      // This should never happen, except grammar changes and we don't notice
      // change in this production.
      debug_assert(false, "can't happen");
      throw std::exception();
    }
  };

  std::string unescaped;
  bool escape = false;

  // First and last char is quote, we don't need to look at them.
  for (int i = 1; i < (int)escaped.size() - 1; ++i) {
    if (escape) {
      switch (escaped[i]) {
        case '\\':
          unescaped += '\\';
          break;
        case '\'':
          unescaped += '\'';
          break;
        case '"':
          unescaped += '"';
          break;
        case 'B':
        case 'b':
          unescaped += '\b';
          break;
        case 'F':
        case 'f':
          unescaped += '\f';
          break;
        case 'N':
        case 'n':
          unescaped += '\n';
          break;
        case 'R':
        case 'r':
          unescaped += '\r';
          break;
        case 'T':
        case 't':
          unescaped += '\t';
          break;
        case 'U':
        case 'u':
          unescaped += EncodeEscapedUnicodeCodepoint(escaped, i);
          break;
        default:
          // This should never happen, except grammar changes and we don't
          // notice change in this production.
          debug_assert(false, "can't happen");
          throw std::exception();
      }
      escape = false;
    } else if (escaped[i] == '\\') {
      escape = true;
    } else {
      unescaped += escaped[i];
    }
  }
  return unescaped;
}

antlrcpp::Any CypherMainVisitor::visitBooleanLiteral(
    CypherParser::BooleanLiteralContext *ctx) {
  if (ctx->getTokens(CypherParser::TRUE).size()) {
    return true;
  } else if (ctx->getTokens(CypherParser::FALSE).size()) {
    return false;
  } else {
    // This should never happen, except grammar changes and we don't
    // notice change in this production.
    debug_assert(false, "can't happen");
    throw std::exception();
  }
}

antlrcpp::Any CypherMainVisitor::visitCypherDelete(
    CypherParser::CypherDeleteContext *ctx) {
  auto *del = storage_.Create<Delete>();
  if (ctx->DETACH()) {
    del->detach_ = true;
  }
  for (auto *expression : ctx->expression()) {
    del->expressions_.push_back(expression->accept(this));
  }
  return del;
}

antlrcpp::Any CypherMainVisitor::visitWhere(CypherParser::WhereContext *ctx) {
  auto *where = storage_.Create<Where>();
  where->expression_ = ctx->expression()->accept(this);
  return where;
}

antlrcpp::Any CypherMainVisitor::visitSet(CypherParser::SetContext *ctx) {
  std::vector<Clause *> set_items;
  for (auto *set_item : ctx->setItem()) {
    set_items.push_back(set_item->accept(this));
  }
  return set_items;
}

antlrcpp::Any CypherMainVisitor::visitSetItem(
    CypherParser::SetItemContext *ctx) {
  // SetProperty
  if (ctx->propertyExpression()) {
    auto *set_property = storage_.Create<SetProperty>();
    set_property->property_lookup_ = ctx->propertyExpression()->accept(this);
    set_property->expression_ = ctx->expression()->accept(this);
    return static_cast<Clause *>(set_property);
  }

  // SetProperties either assignment or update
  if (ctx->getTokens(kPropertyAssignmentTokenId).size() ||
      ctx->getTokens(kPropertyUpdateTokenId).size()) {
    auto *set_properties = storage_.Create<SetProperties>();
    set_properties->identifier_ = storage_.Create<Identifier>(
        ctx->variable()->accept(this).as<std::string>());
    set_properties->expression_ = ctx->expression()->accept(this);
    if (ctx->getTokens(kPropertyUpdateTokenId).size()) {
      set_properties->update_ = true;
    }
    return static_cast<Clause *>(set_properties);
  }

  // SetLabels
  auto *set_labels = storage_.Create<SetLabels>();
  set_labels->identifier_ = storage_.Create<Identifier>(
      ctx->variable()->accept(this).as<std::string>());
  set_labels->labels_ =
      ctx->nodeLabels()->accept(this).as<std::vector<GraphDb::Label>>();
  return static_cast<Clause *>(set_labels);
}

antlrcpp::Any CypherMainVisitor::visitPropertyExpression(
    CypherParser::PropertyExpressionContext *ctx) {
  Expression *expression = ctx->atom()->accept(this);
  for (auto *lookup : ctx->propertyLookup()) {
    auto property_lookup =
        storage_.Create<PropertyLookup>(expression, lookup->accept(this));
    expression = property_lookup;
  }
  // It is guaranteed by grammar that there is at least one propertyLookup.
  return dynamic_cast<PropertyLookup *>(expression);
}
}
}
