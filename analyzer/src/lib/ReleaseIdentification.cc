#include "ReleaseIdentification.hh"
#include "llvm/IR/InstIterator.h"

#include <algorithm>

bool ReleaseIdentificationPass::doModulePass(llvm::Module *M) {
  for (auto& func : *M) {
    for (inst_iterator i = inst_begin(func), e = inst_end(func);
         i != e; ++i) {
      if (CallInst *call_ins = dyn_cast<CallInst>(&*i)) {
        Function *called_fun = call_ins->getCalledFunction();
        if (called_fun != nullptr) {
          StringRef name = called_fun->getName();
          if (name.contains_insensitive("free") ||
              name.contains_insensitive("release")) {
            if (callee_times.count(name))
              callee_times[name]++;
            else
              callee_times[name] = 1;
          }
        }
      }
    }
  }
  return false;
}


bool ReleaseIdentificationPass::doInitialization(llvm::Module *M) {
  return false;
}

bool ReleaseIdentificationPass::doFinalization(llvm::Module *M) {
  return false;
}

void ReleaseIdentificationPass::output() {
  multimap<int, StringRef> sorted_callee;
  for (auto map : callee_times) {
    sorted_callee.insert(std::make_pair(map.second, map.first));
  }
  for (auto map : sorted_callee) {
    OP << map.first << ": " << map.second << "\n";
  }
}
