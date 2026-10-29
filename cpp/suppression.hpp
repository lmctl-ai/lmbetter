#pragma once
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace lmok {
// Byte/line based, intentionally independent of comment grammar. Only the
// finding's starting line and the preceding line can suppress it.
class Suppression {
  struct Directive { bool all=false; std::set<std::string> ids; bool empty=false; };
  std::map<size_t, Directive> lines_;
  std::set<std::string> observed_;
  bool warned_=false;
  static bool word_char(unsigned char c) { return std::isalnum(c)||c>=128||c=='_'||c=='-'||c=='.'; }
  static bool id_char(unsigned char c) { return word_char(c)||c==':'||c=='/'; }
public:
  size_t suppressed=0;
  explicit Suppression(const std::string &source) {
    size_t row=0,start=0;
    while(start<=source.size()) {
      size_t end=source.find('\n',start);if(end==std::string::npos)end=source.size();
      const std::string line=source.substr(start,end-start);
      size_t at=0;
      while((at=line.find("lmok-disable",at))!=std::string::npos) {
        size_t after=at+12;
        if((at&&word_char(static_cast<unsigned char>(line[at-1])))||(after<line.size()&&word_char(static_cast<unsigned char>(line[after])))){at=after;continue;}
        auto &d=lines_[row];size_t p=after;
        while(p<line.size()&&(line[p]==' '||line[p]=='\t'))++p;
        if(p==line.size()||line[p]!=':')d.all=true;
        else {
          ++p;bool found=false;
          for(;;){while(p<line.size()&&(line[p]==' '||line[p]=='\t'))++p;size_t first=p;
            while(p<line.size()&&id_char(static_cast<unsigned char>(line[p])))++p;
            if(first==p)break;
            d.ids.insert(line.substr(first,p-first));found=true;
            while(p<line.size()&&(line[p]==' '||line[p]=='\t'))++p;
            if(p==line.size()||line[p]!=',')break;
            ++p;
          }
          if(!found)d.empty=true;
        }
        at=after;
      }
      if(end==source.size())break;
      start=end+1;++row;
    }
  }
  bool drop(size_t row,const std::string &rule,bool disabled=false) {
    if(!rule.empty())observed_.insert(rule);
    if(disabled)return false;
    for(size_t r: {row,row?row-1:row}){auto i=lines_.find(r);if(i!=lines_.end()&&(i->second.all||i->second.ids.count(rule))){++suppressed;return true;}}
    return false;
  }
  void warn(const std::string &path,const std::set<std::string> &known={},bool observed_only=false) {
    if(warned_)return;
    warned_=true;
    for(const auto &[row,d]:lines_) {
      if(d.empty)std::cerr<<"warning:"<<path<<":"<<row+1<<": empty lmok-disable rule list\n";
      for(const auto &id:d.ids)if(!known.count(id)&&!observed_.count(id))
        std::cerr<<"warning:"<<path<<":"<<row+1<<": unknown rule id '"<<id<<"' in lmok-disable"<<(observed_only?" (not observed in this check sink)":"")<<"\n";
    }
  }
};
inline std::string suppressed_suffix(size_t count) { return count?" ("+std::to_string(count)+" suppressed)":""; }
} // namespace lmok
