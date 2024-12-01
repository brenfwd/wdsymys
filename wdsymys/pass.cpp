#include <iostream>

#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

// #include <unordered_set>

// #include "RegisterPackerContext.h"

namespace {
struct WDSYMYSPass : public llvm::PassInfoMixin<WDSYMYSPass> {
  // RegisterPackerContext create_register_packer_context(llvm::Module& module) {
  //   RegisterPackerContext context{
  //       .pointer_size = module.getDataLayout().getPointerSizeInBits(),
  //   };

  //   return context;
  // }

  llvm::PreservedAnalyses run(llvm::Function& F,
                              llvm::FunctionAnalysisManager& FAM) {
    llvm::LazyValueInfo& LVI = FAM.getResult<llvm::LazyValueAnalysis>(F);

    for (auto& BB : F) {
      llvm::errs() << "Basic Block: " << BB.getName() << "\n";

      // Iterate through each instruction in the basic block
      for (auto& I : BB) {
        llvm::errs() << "Instruction: " << I << "\n";

        if (auto* Load = dyn_cast<llvm::LoadInst>(&I)) {
          llvm::Value* V = Load;
          llvm::ConstantRange CR = LVI.getConstantRange(V, &BB);

          llvm::errs() << "Instruction:\t" << I << "\n";
          llvm::errs() << "Value range:\t" << CR << "\n";
        }

        // Iterate through operands of the instruction
        for (auto& U : I.operands()) {
          auto* V = U.get();
          if (!V->getType()->isIntegerTy() &&
              !V->getType()->isFloatingPointTy()) {
            llvm::errs() << "Skipping instruction: " << I << "\n";
            continue;
          }

          llvm::errs() << "Value range: " << LVI.getConstantRange(V, &I, true)
                       << "\n";
        }
      }
    }
    return llvm::PreservedAnalyses::all();
  }
};

}  // namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "HW2Pass", "v0.1",
          [](llvm::PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager& FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "wdsymys") {
                    FPM.addPass(WDSYMYSPass());
                    return true;
                  }
                  return false;
                });
          }};
}
