#include "FunctionPathsPass.hh"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constant.h"
#include "llvm/ADT/Hashing.h"
#include "queue"


void FunctionPathsPass::dfs_on_function(BasicBlock *bb,
                                        FunctionPath *fp, FunctionPaths *fps,
                                        int &paths_length) {
  if (paths_length > PATHLIMIT)
    return;
  fp->path.push_back(bb);
  fp->path_set.insert(bb);

  Instruction *last_ins = bb->getTerminator();
  if (BranchInst *br_ins = dyn_cast<BranchInst>(last_ins)) {
    if (br_ins->isUnconditional()) {
      if (fp->path_set.find(br_ins->getSuccessor(0)) == fp->path_set.end()) {
        dfs_on_function(br_ins->getSuccessor(0),
                        fp, fps, paths_length);
      } else {
        paths_length++;
      }
    }
    else {
      if (fp->path_set.find(br_ins->getSuccessor(0)) ==
          fp->path_set.end()) {
        fp->bb_direction[br_ins->getParent()] = true;
        dfs_on_function(br_ins->getSuccessor(0),
                        fp, fps, paths_length);
        fp->bb_direction.erase(br_ins->getParent());
      } else {
        paths_length++;
      }
      if (fp->path_set.find(br_ins->getSuccessor(1)) ==
          fp->path_set.end()) {
        fp->bb_direction[br_ins->getParent()] = false;
        dfs_on_function(br_ins->getSuccessor(1),
                        fp, fps, paths_length);
        fp->bb_direction.erase(br_ins->getParent());
      } else {
        paths_length++;
      }
    }
  }

  if (CallBrInst *callbr_ins = dyn_cast<CallBrInst>(last_ins)) {
    if (fp->path_set.find(callbr_ins->getDefaultDest())
        == fp->path_set.end()) {
      dfs_on_function(callbr_ins->getDefaultDest(),
                      fp, fps, paths_length);
    } else {
      paths_length++;
    }
    for (unsigned i = 0; i < callbr_ins->getNumIndirectDests(); i++) {
       if (fp->path_set.find(callbr_ins->getIndirectDest(i))
           == fp->path_set.end()) {
         dfs_on_function(callbr_ins->getIndirectDest(i),
                         fp, fps, paths_length);
       } else {
         paths_length++;
       }
    }
  }

  if (SwitchInst *switch_ins = dyn_cast<SwitchInst>(last_ins)) {
    for (unsigned int i = 0; i < switch_ins->getNumSuccessors(); i++) {
      if (fp->path_set.find(switch_ins->getSuccessor(i))
          == fp->path_set.end()) {
        dfs_on_function(switch_ins->getSuccessor(i),
                        fp, fps, paths_length);
      } else {
        paths_length++;
      }
    }
  }

  if (ReturnInst *r_ins = dyn_cast<ReturnInst>(last_ins)) {
    if (fps->size() > PATHLIMIT)
      return;
    FunctionPath *tmp = new FunctionPath(*fp);
    fps->insert(tmp);
    paths_length++;
  }

  fp->path.pop_back();
  fp->path_set.erase(bb);
}

FunctionPaths *FunctionPathsPass::make_function_paths(Function *f) {
  PEG::PEGNode *ret = nullptr;
  if (Ctx->func_retnode_map.count(f))
    ret = Ctx->func_retnode_map[f];
  FunctionPath f_path(f, &Ctx->arg_node_map, ret);
  FunctionPaths *f_paths = new FunctionPaths;
  int paths_length = 0;
  dfs_on_function(&f->getEntryBlock(), &f_path, f_paths, paths_length);
  return f_paths;
}


bool FunctionPathsPass::doModulePass(llvm::Module *M) {
  for (Module::iterator F = M->begin(); F != M->end(); F++) {
    Function *curr_f = &*F;
    if (!curr_f->empty()) {
      OP << KGRN << curr_f->getName();
      FunctionPaths *f_paths = make_function_paths(curr_f);
      if (f_paths->size() < PATHLIMIT) {
        Ctx->plain_func_map[curr_f] = f_paths;
        for (auto fp : *f_paths) {
          for (auto bb : fp->path) {
            fp->bb_edges_map[bb] = &Ctx->bb_edges_map[bb];
          }
          fp->update_phi_set();
        }
      }
      OP<< KRED << " " << f_paths->size() << "\n" << KNRM;
    }
  }
  return false;
}

bool FunctionPathsPass::doInitialization(llvm::Module *M) {
  for (Module::iterator F = M->begin(); F != M->end(); F++) {
    Function *curr_f = &*F;
    if (curr_f->getReturnType()->isVoidTy())
      continue;
    if (curr_f->getName().contains_insensitive("bio"))
      // bio is memory pool related
      continue;

    bool kfree_flag = false;
    for (inst_iterator i = inst_begin(curr_f), e = inst_end(curr_f);
         i != e; ++i) {
      if (CallInst *call_ins = dyn_cast<CallInst>(&*i)) {
        Function *called_fun = call_ins->getCalledFunction();
        if (called_fun != NULL &&
            Ctx->release_funcs.count(called_fun->getName().str())) {
          kfree_flag = true;
          break;
        }
        if (called_fun != NULL &&
            called_fun->getName().contains("CRYPTO_DOWN_REF")) {
		  // inconsistency caused by ref count does not lead to security problem;
		  // this case should not take into consderation.
          Ctx->refcnt_fun_set.insert(curr_f);
        }
      }
    }
    if (kfree_flag) {
      Ctx->callee_set.insert(curr_f);
      Ctx->kfree_fun_set.insert(curr_f);
    }
  }

   for (Module::iterator F = M->begin(); F != M->end(); F++) {
     Function *curr_f = &*F;
     if (curr_f->getReturnType()->isVoidTy())
       continue;
     if (!curr_f->getReturnType()->isPointerTy())
       continue;
     if (curr_f->getName().contains_insensitive("bio"))
       // bio is memory pool related, should not be considered
       continue;

     bool ret_flag = true;
     for (inst_iterator i = inst_begin(curr_f), e = inst_end(curr_f);
          i != e; ++i) {
       if (CallInst *call_ins = dyn_cast<CallInst>(&*i)) {
         Function *called_fun = call_ins->getCalledFunction();
         if (called_fun != NULL &&
             called_fun->getName() == "zend_throw_error") {
		   // error throwing will crash whole program, making inconsistency meaningless
           ret_flag = false;
           break;
         }
       }
     }
     if (ret_flag) {
       Ctx->callee_set.insert(curr_f);
       Ctx->retval_fun_set.insert(curr_f);
     }
   }

   for (Module::iterator F = M->begin(); F != M->end(); F++) {
     Function *curr_f = &*F;
     if (curr_f->getReturnType()->isVoidTy())
       continue;
     bool init_flag = false;
     for (inst_iterator i = inst_begin(curr_f), e = inst_end(curr_f);
          i != e; ++i) {
       if (CallInst *call_ins = dyn_cast<CallInst>(&*i)) {
         Function *called_fun = call_ins->getCalledFunction();
         if (called_fun != NULL &&
             called_fun->getName().contains("memset")) {
           init_flag = true;
           break;
         }
       }
     }
     if (init_flag) {
       Ctx->callee_set.insert(curr_f);
       Ctx->init_fun_set.insert(curr_f);
     }
   }

   return false;
}

bool FunctionPathsPass::doFinalization(llvm::Module *M) {
  return false;
}
