#ifndef DISTINGUISHER_H
#define DISTINGUISHER_H

#include "PEGNode.hh"
#include "RIDCommon.hh"

class Distinguisher {
private:
  bool collect_retval(FunctionPath *, FunctionPath *,
                      PEG::SmallPEGNodeSet *);
  bool collect_global(FunctionPath *, FunctionPath *,
                      PEG::SmallPEGNodeSet *);
  bool collect_condition(FunctionPath *, FunctionPath *,
                         PEG::SmallPEGNodeSet *);
  std::map<PEG::PEGNode *, int> opt_map;
public:
  bool retval;
  Distinguisher(bool _retval) : retval(_retval) {};
  bool collect_distinguisher(FunctionPath *, FunctionPath *,
                             PEG::SmallPEGNodeSet *);
};

#endif
