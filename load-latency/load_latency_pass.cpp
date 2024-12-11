#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

#include <vector>
#include <unordered_set>

#include "load_latency_pass.h"

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "LoadLatency", "v0.1", [](llvm::PassBuilder& PB) {
        PB.registerPipelineParsingCallback(
            [](llvm::StringRef Name, llvm::FunctionPassManager& FPM,
               llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
              if (Name == "load-latency") {
                FPM.addPass(LoadLatencyPass());
                // FPM.addPass(llvm::VerifierPass());
                return true;
              }
              return false;
            });
        PB.registerOptimizerLastEPCallback([&](llvm::ModulePassManager& MPM,
                                               auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(LoadLatencyPass()));
          return true;
        });

//        PB.registerPipelineEarlySimplificationEPCallback(
//            [&](llvm::ModulePassManager& MPM, auto) {
//              MPM.addPass(createModuleToFunctionPassAdaptor(
//                  llvm::PromotePass()));
//              MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSLVIPass()));
//              MPM.addPass(createModuleToFunctionPassAdaptor(
//                  WDSYMYSPackingPass()));
//              MPM.addPass(createModuleToFunctionPassAdaptor(llvm::VerifierPass()));
//              return true;
//        });
    }};
}
