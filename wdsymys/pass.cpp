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
      llvm::errs() << BB << "\n\n";

      // Iterate through each instruction in the basic block
      for (auto& I : BB) {
        // llvm::errs() << "Instruction: " << I << "\n";

        // Iterate through operands of the instruction
        llvm::Value* V = &I;
        if (!V->getType()->isIntegerTy() &&
            !V->getType()->isFloatingPointTy()) {
          continue;
        }

        llvm::errs() << "INSTR:              " << I << "\n"
                     << "CONSTANT RANGE:     "
                     << LVI.getConstantRange(V, &I, true) << "\n\n";
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
