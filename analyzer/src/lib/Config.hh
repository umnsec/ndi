#ifndef SACONFIG_H
#define SACONFIG_H

#include "llvm/Support/FileSystem.h"
#include "Common.hh"

#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <fstream>

//
// Configurations for compilation.
//
//#define SOUND_MODE 1
#define MLTA_FOR_INDIRECT_CALL

// Skip functions with more blocks to avoid scalability issues
#define MAX_BLOCKS_SUPPORT 500

//
// Function modeling
//

static void
SetReleaseFuncs(set<string> &release_funcs) {
  string exepath = sys::fs::getMainExecutable(NULL, NULL);
  string exedir = exepath.substr(0, exepath.find_last_of('/'));
  string line;
  ifstream errfile(exedir + "/configs/release-funcs");
  if (errfile.is_open()) {
    while (!errfile.eof()) {
      getline(errfile, line);
      if (line.length() > 1) {
        release_funcs.insert(line);
      }
    }
    errfile.close();
  }
}

// Setup functions that handle errors
static void SetErrorHandleFuncs(set<string> &ErrorHandleFuncs) {

  string exepath = sys::fs::getMainExecutable(NULL, NULL);
  string exedir = exepath.substr(0, exepath.find_last_of('/'));
  string line;
  ifstream errfile(exedir + "/configs/err-funcs");
  if (errfile.is_open()) {
    while (!errfile.eof()) {
      getline (errfile, line);
      if (line.length() > 1) {
        ErrorHandleFuncs.insert(line);
      }
    }
    errfile.close();
  }

  string ErrorHandleFN[] = {
    "BUG",
    "BUG_ON",
    "ASM_BUG",
    "panic",
    "ASSERT",
    "assert",
    "dump_stack",
    "__warn_printk",
    "usercopy_warn",
    "signal_fault",
    "pr_err",
    "pr_warn",
    "pr_warning",
    "pr_alert",
    "pr_emerg",
    "pr_crit",
  };
  for (auto F : ErrorHandleFN) {
    ErrorHandleFuncs.insert(F);
  }
}

// Read module dependency
static void SetModuleDependency(std::map
                                <std::string, std::set<std::string>>&
                                module_dependency) {

  string exepath = sys::fs::getMainExecutable(NULL, NULL);
  string exedir = exepath.substr(0, exepath.find_last_of('/'));
  string key, value;
  ifstream errfile(exedir + "/configs/module-dependency");
  if (errfile.is_open()) {
    while (!errfile.eof()) {
      getline(errfile, key);
      getline(errfile, value);
      while (value != "") {
        module_dependency[key].insert(value);
        getline(errfile, value);
      }
    }
    errfile.close();
  }
}

// Setup functions that copy/move/cast values.
static void SetCopyFuncs(// <src, dst, size>
                         map<string, tuple<int8_t, int8_t, int8_t>> &CopyFuncs) {
  CopyFuncs["memcpy"] = make_tuple(1, 0, 2);
  CopyFuncs["__memcpy"] = make_tuple(1, 0, 2);
  CopyFuncs["llvm.memcpy.p0i8.p0i8.i32"] = make_tuple(1, 0, 2);
  CopyFuncs["llvm.memcpy.p0i8.p0i8.i64"] = make_tuple(1, 0, 2);
  CopyFuncs["strncpy"] = make_tuple(1, 0, 2);
  CopyFuncs["memmove"] = make_tuple(1, 0, 2);
  CopyFuncs["__memmove"] = make_tuple(1, 0, 2);
  CopyFuncs["llvm.memmove.p0i8.p0i8.i32"] = make_tuple(1, 0, 2);
  CopyFuncs["llvm.memmove.p0i8.p0i8.i64"] = make_tuple(1, 0, 2);
}

#endif
