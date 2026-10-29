#pragma once
// lmok test is a pack-author oracle: exact (one-based line, rule id) sets.
// Only real CST comment tokens count; a marker inside a string is never an annotation.
#include "lint.hpp"
#include <set>
#include <sstream>
namespace lmok {
struct RuleTestDiff {std::string file,rule,kind;int64_t line;};
struct RuleTestResult {size_t expected=0,actual=0;std::vector<RuleTestDiff> diff;};
inline RuleTestResult test_rule_file(const std::vector<LintRule> &rules,Grammar grammar,const std::string &path,const std::string &source){
  std::set<std::pair<int64_t,std::string>> expected,actual;
  const Tree tree=parse(grammar,source);
  for(const auto &node:tree_nodes(tree).items){
    if(node_kind(node).find("comment")==std::string::npos)continue;
    const auto end=static_cast<size_t>(node_end_byte(node));
    const auto eol=source.find('\n',end);
    if(source.substr(end,eol==std::string::npos?eol:eol-end).find_first_not_of(" \t\r")!=std::string::npos)continue;
    auto text=node_text(node);const std::string token="lmok-expect:";const auto mark=text.find(token);
    if(mark==std::string::npos)continue;
    if(mark&&std::string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-").find(text[mark-1])!=std::string::npos)continue;
    const int64_t row=node_start_row(node)+std::count(text.begin(),text.begin()+mark,'\n')+1;
    auto ids=text.substr(mark+token.size());
    if(ids.size()>=2&&ids.substr(ids.size()-2)=="*/")ids.resize(ids.size()-2);
    std::stringstream list(ids);std::string id;bool any=false;
    while(std::getline(list,id,',')){
      const auto first=id.find_first_not_of(" \t\r");const auto last=id.find_last_not_of(" \t\r");
      id=first==std::string::npos?"":id.substr(first,last-first+1);
      if(id.empty()||id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=std::string::npos)
        throw dslvm::Error(path+":"+std::to_string(row)+": invalid lmok-expect rule list");
      expected.insert({row,id});any=true;
    }
    if(!any||(!ids.empty()&&ids.back()==','))throw dslvm::Error(path+":"+std::to_string(row)+": empty lmok-expect rule");
  }
  // No ignores/suppression/baseline: they must not make an oracle silently clean.
  for(const auto &finding:lint_text(rules,source))actual.insert({finding.row+1,finding.rule});
  RuleTestResult result;result.expected=expected.size();result.actual=actual.size();
  for(const auto &item:expected)if(!actual.count(item))result.diff.push_back({path,item.second,"missing",item.first});
  for(const auto &item:actual)if(!expected.count(item))result.diff.push_back({path,item.second,"unexpected",item.first});
  return result;
}
inline std::string rule_test_json(const RuleTestResult &result){
  std::string out="{\"ok\":"+std::string(result.diff.empty()?"true":"false")+",\"expected\":"+std::to_string(result.expected)+",\"actual\":"+std::to_string(result.actual)+",\"diff\":[";
  bool first=true;for(const auto &d:result.diff){if(!first)out+=',';first=false;out+="{\"file\":"+meta_quote(d.file)+",\"line\":"+std::to_string(d.line)+",\"rule\":"+meta_quote(d.rule)+",\"kind\":"+meta_quote(d.kind)+"}";}return out+"]}";
}
}
