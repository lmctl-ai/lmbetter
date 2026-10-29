#pragma once
// Shared by pack findings and ts.report_meta. SARIF writers should use these
// fields for rule properties/helpUri; never derive metadata from finding text.
#include "dslvm.hpp"
#include "stdlib/json.hpp"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
namespace lmok {
inline std::string meta_quote(const std::string &text) {
  std::string out="\""; const char *hex="0123456789abcdef";
  for(unsigned char c:text) { if(c=='"'||c=='\\'){out+='\\';out+=char(c);} else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c); }
  return out+'"';
}
struct RuleMetadata {
  std::map<std::string,std::vector<std::string>> lists;
  std::map<std::string,std::string> strings;
  bool empty() const {return lists.empty()&&strings.empty();}
  std::string json() const {
    std::string out="{";bool first=true;
    for(const auto &entry:lists){if(!first)out+=',';first=false;out+=meta_quote(entry.first)+":[";bool item=true;for(const auto &s:entry.second){if(!item)out+=',';item=false;out+=meta_quote(s);}out+=']';}
    for(const auto &entry:strings){if(!first)out+=',';first=false;out+=meta_quote(entry.first)+':'+meta_quote(entry.second);}return out+'}';
  }
};
inline RuleMetadata read_rule_metadata(const dslvm::stdlib::JsonNode &object) {
  using J=dslvm::stdlib::JsonNode;
  if(object.kind!=J::Kind::Object)throw dslvm::Error("metadata must be an object");
  RuleMetadata out;
  for(const auto &entry:object.object){
    const auto &key=entry.key;const auto &value=*entry.value;
    if(key=="cwe"||key=="owasp"||key=="tags"){
      auto &items=out.lists[key];
      const auto add=[&](const J &item){if(item.kind!=J::Kind::String||item.text.empty())throw dslvm::Error(key+" must contain non-empty strings");items.push_back(item.text);};
      if(value.kind==J::Kind::Array)for(const auto &item:value.array)add(item);
      else if(key!="tags"&&value.kind==J::Kind::String)add(value);
      else throw dslvm::Error(key+" must be an array of strings");
      std::sort(items.begin(),items.end());items.erase(std::unique(items.begin(),items.end()),items.end());
    }else if(key=="url"||key=="confidence"){
      if(value.kind!=J::Kind::String||value.text.empty())throw dslvm::Error(key+" must be a non-empty string");
      if(key=="confidence"&&value.text!="low"&&value.text!="medium"&&value.text!="high")throw dslvm::Error("confidence must be low, medium or high");
      out.strings[key]=value.text;
    }
  }
  return out;
}
inline RuleMetadata parse_rule_metadata(const std::string &text) {
  dslvm::stdlib::json_detail::Parser parser(text);auto object=parser.val();parser.ws();
  if(parser.p!=text.size())throw dslvm::Error("metadata has trailing JSON bytes");
  return read_rule_metadata(object);
}
inline std::string metadata_suffix(const RuleMetadata &metadata,bool requested) {
  return requested&&!metadata.empty()?",\"metadata\":"+metadata.json():"";
}
}
