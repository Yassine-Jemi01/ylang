#ifndef YLANG_LLVM_H
#define YLANG_LLVM_H

#include "ylang/ir.h"

/* Emit textual LLVM IR for the verified scalar YLang IR subset. */
bool ylang_llvm_write_ir(const YLIRModule *module, FILE *output,
                         FILE *diagnostics);

#endif
