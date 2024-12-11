#pragma once

#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h>

struct LoadLatencyPass : public llvm::PassInfoMixin<LoadLatencyPass> {
  static char ID;
  llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM) {
    llvm::LLVMContext &Ctx = F.getContext();
    llvm::Module *M = F.getParent();
    llvm::IRBuilder<> Builder(Ctx);

    llvm::Type *Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::StructType *TimespecTy = llvm::StructType::create(Ctx, {Int64Ty, Int64Ty}, "timespec");

    // just like, extern this, maybe a linker problem?
    llvm::FunctionCallee NanoSleepFunc = M->getOrInsertFunction(
        "nanosleep",
        llvm::FunctionType::get(llvm::Type::getInt32Ty(Ctx),
            {TimespecTy->getPointerTo(), TimespecTy->getPointerTo()},
            false));

    for (llvm::BasicBlock &BB : F) {
      for (llvm::Instruction &I : BB) {
        if (llvm::isa<llvm::LoadInst>(&I)) {
          Builder.SetInsertPoint(&I);

          // man page says this function is dumb and no like raw number
          llvm::AllocaInst *Timespec = Builder.CreateAlloca(TimespecTy);
          Builder.CreateStore(
              llvm::ConstantStruct::get(TimespecTy, {
                  Builder.getInt64(0),
                  Builder.getInt64(10)
              }),
              Timespec);

          Builder.CreateCall(NanoSleepFunc, {Timespec, llvm::ConstantPointerNull::get(Timespec->getType())});
        }
      }
    }
    return llvm::PreservedAnalyses::none();
  }
};