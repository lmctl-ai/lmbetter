// Rule-pack loading and the lint dispatcher; see lint.hpp for the contract.
#include "lint.hpp"
#include "dslvm.hpp"
#include "stdlib/json.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace lmbetter {
namespace {

using dslvm::stdlib::JsonNode;

const JsonNode *member(const JsonNode &obj, const char *key) {
  if (obj.kind != JsonNode::Kind::Object) return nullptr;
  for (const auto &m : obj.object)
    if (m.key == key) return m.value.get();
  return nullptr;
}

std::string pack_error(const std::string &path, const std::string &what) {
  return "rules pack " + path + ": " + what;
}

bool grammar_of(const std::string &name, Grammar &grammar) {
  if (name == "json") { grammar = Grammar::Json; return true; }
  if (name == "c") { grammar = Grammar::C; return true; }
  return false;
}

std::string expand_message(const std::string &message, const std::string &text) {
  std::string out;
  size_t at = 0;
  for (;;) {
    const size_t mark = message.find("{text}", at);
    if (mark == std::string::npos) return out + message.substr(at);
    out += message.substr(at, mark - at) + text;
    at = mark + 6;
  }
}

void maybe_emit(const LintRule &rule, const Node &node, std::vector<LintFinding> &findings) {
  const std::string text = node_text(node);
  if (!rule.text_matches.empty() && !std::regex_search(text, rule.text_regex)) return;
  findings.push_back({node_start_row(node), node_start_col(node),
                      node_end_row(node), node_end_col(node),
                      rule.id, rule.severity, expand_message(rule.message, text)});
}

} // namespace

std::vector<LintRule> load_rule_pack(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw dslvm::Error("cannot read " + path);
  std::ostringstream data;
  data << in.rdbuf();
  const std::string text = data.str();

  JsonNode root;
  try {
    dslvm::stdlib::json_detail::Parser parser(text);
    root = parser.val();
    parser.ws();
    if (parser.p != text.size()) throw dslvm::Error("trailing bytes");
  } catch (const dslvm::Error &e) {
    throw dslvm::Error(pack_error(path, "invalid JSON (" + std::string(e.what()) + ")"));
  }
  if (root.kind != JsonNode::Kind::Object)
    throw dslvm::Error(pack_error(path, "top level must be an object"));

  Grammar pack_grammar = Grammar::Json;
  bool pack_has_lang = false;
  if (const JsonNode *lang = member(root, "lang")) {
    if (lang->kind != JsonNode::Kind::String || !grammar_of(lang->text, pack_grammar))
      throw dslvm::Error(pack_error(path, "unknown lang '" +
        (lang->kind == JsonNode::Kind::String ? lang->text : "?") + "' (known: json, c)"));
    pack_has_lang = true;
  }
  const JsonNode *rules_node = member(root, "rules");
  if (!rules_node || rules_node->kind != JsonNode::Kind::Array)
    throw dslvm::Error(pack_error(path, "missing 'rules' array"));

  std::vector<LintRule> rules;
  size_t index = 0;
  for (const auto &entry : rules_node->array) {
    ++index;
    const std::string who = "rule " + [&] {
      if (const JsonNode *id = member(entry, "id"))
        if (id->kind == JsonNode::Kind::String && !id->text.empty()) return id->text;
      return "#" + std::to_string(index);
    }();
    const auto fail = [&](const std::string &what) {
      throw dslvm::Error(pack_error(path, who + ": " + what));
    };
    LintRule rule;
    const JsonNode *id = member(entry, "id");
    if (!id || id->kind != JsonNode::Kind::String || id->text.empty())
      fail("missing 'id'");
    rule.id = id->text;
    const JsonNode *on = member(entry, "on");
    if (on) {
      if (on->kind != JsonNode::Kind::String || on->text.empty()) fail("'on' must be a node-kind string");
      rule.on = on->text;
    }
    const JsonNode *query = member(entry, "query");
    if (query) {
      if (query->kind != JsonNode::Kind::String || query->text.empty()) fail("'query' must be a pattern string");
      rule.query = query->text;
    }
    if (!on && !query) fail("needs 'on' or 'query'");
    if (on && query) fail("'on' and 'query' are mutually exclusive (pick one selector)");
    const JsonNode *message = member(entry, "message");
    if (!message || message->kind != JsonNode::Kind::String)
      fail("missing 'message'");
    rule.message = message->text;
    if (const JsonNode *severity = member(entry, "severity")) {
      if (severity->kind != JsonNode::Kind::String || severity->text.empty())
        fail("'severity' must be a string");
      rule.severity = severity->text;
    }
    if (const JsonNode *lang = member(entry, "lang")) {
      if (lang->kind != JsonNode::Kind::String || !grammar_of(lang->text, rule.grammar))
        fail("unknown lang '" + (lang->kind == JsonNode::Kind::String ? lang->text : "?") +
             "' (known: json, c)");
    } else {
      if (!pack_has_lang) fail("no language (set 'lang' at pack or rule level)");
      rule.grammar = pack_grammar;
    }
    if (const JsonNode *tm = member(entry, "text_matches")) {
      if (tm->kind != JsonNode::Kind::String || tm->text.empty())
        fail("'text_matches' must be a regex string");
      rule.text_matches = tm->text;
      try {
        rule.text_regex = std::regex(rule.text_matches); // ECMAScript
      } catch (const std::regex_error &e) {
        fail("bad text_matches regex '" + rule.text_matches + "'");
      }
    }
    rules.push_back(std::move(rule));
  }
  return rules;
}

std::vector<LintFinding> lint_text(const std::vector<LintRule> &rules, const std::string &text) {
  std::vector<LintFinding> findings;
  for (const Grammar grammar : {Grammar::Json, Grammar::C}) {
    std::vector<const LintRule *> here;
    for (const auto &rule : rules)
      if (rule.grammar == grammar) here.push_back(&rule);
    if (here.empty()) continue;
    const Tree tree = parse(grammar, text);
    const Node root = node_root(tree);

    // One pre-order walk, dispatched through the kind index (the `on` path —
    // the piece a v6 rule entry table will reuse for annotated lmcode fns).
    std::map<std::string, std::vector<const LintRule *>> by_kind;
    for (const LintRule *rule : here)
      if (!rule->on.empty()) by_kind[rule->on].push_back(rule);
    if (!by_kind.empty()) {
      const NodeList all = tree_nodes(tree); // named nodes only
      for (const Node &node : all.items) {
        const auto it = by_kind.find(node_kind(node));
        if (it == by_kind.end()) continue;
        for (const LintRule *rule : it->second) maybe_emit(*rule, node, findings);
      }
    }

    // One query cursor per query rule (later optimization: fold into the walk).
    for (const LintRule *rule : here) {
      if (rule->query.empty()) continue;
      NodeList captures;
      try {
        captures = node_query(root, rule->query, grammar);
      } catch (const QueryError &e) {
        throw QueryError("rule " + rule->id + ": " + e.what());
      }
      for (const Node &node : captures.items) maybe_emit(*rule, node, findings);
    }
  }
  std::stable_sort(findings.begin(), findings.end(), [](const LintFinding &a, const LintFinding &b) {
    return a.row < b.row || (a.row == b.row && a.col < b.col);
  });
  return findings;
}

} // namespace lmbetter
