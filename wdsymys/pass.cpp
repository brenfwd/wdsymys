#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

// #include "RegisterPackerContext.h"

namespace {
struct WDSYMYSPacking {
  int bits_remaining = 64;
  llvm::SmallVector<llvm::Value*, 4> to_pack;
};


struct WDSYMYSPass : public llvm::PassInfoMixin<WDSYMYSPass> {
  // RegisterPackerContext create_register_packer_context(llvm::Module& module) {
  //   RegisterPackerContext context{
  //       .pointer_size = module.getDataLayout().getPointerSizeInBits(),
  //   };

  //   return context;
  // }

private:

  llvm::Value *doPack32(llvm::SmallVector<llvm::Value*, 4> &ToPack, llvm::IRBuilder<> &Builder, llvm::LLVMContext &Ctx) {
    llvm::Type *I64 = llvm::Type::getInt64Ty(Ctx);
    llvm::Value *PackedValue = llvm::ConstantInt::get(I64, 0);

    for (size_t i = 0; i < ToPack.size(); i++) {
      Builder.SetInsertPoint(static_cast<llvm::Instruction*>(ToPack[i])->getNextNode());
      llvm::Value *Ext = Builder.CreateZExt(ToPack[i], I64, "ext");
      llvm::Value *Shifted = Builder.CreateShl(Ext, llvm::ConstantInt::get(I64, i * 32), "shifted");
      PackedValue = Builder.CreateOr(PackedValue, Shifted, "packed");
    }

    return PackedValue;
  }

  llvm::Value* doUnpack32(llvm::Value* Packed, size_t Index, llvm::IRBuilder<>& Builder,
                      llvm::LLVMContext& Context) {
    llvm::Type* I64 = llvm::Type::getInt64Ty(Context);
    llvm::Type* I32 = llvm::Type::getInt32Ty(Context);
  
    Builder.SetInsertPoint(static_cast<llvm::Instruction*>(Packed)->getNextNode());
     llvm::Value* Shifted = Builder.CreateLShr(
        Packed, llvm::ConstantInt::get(I64, Index * 32), "unpack_shifted");
    return Builder.CreateTrunc(Shifted, I32, "unpacked");
  }

public:
  llvm::PreservedAnalyses run(llvm::Function& F,
                              llvm::FunctionAnalysisManager& FAM) {
    llvm::LazyValueInfo& LVI = FAM.getResult<llvm::LazyValueAnalysis>(F);

    llvm::LLVMContext& Ctx = F.getContext();
    llvm::IRBuilder<> Builder(Ctx);
    llvm::DenseMap<llvm::Value*, std::pair<llvm::Value*, size_t>> PackMap;
    

    for (auto& BB : F) {
      llvm::SmallVector<llvm::Value*, 4> ToPack;
      
      llvm::errs() << "Basic Block: " << BB.getName() << "\n";
      llvm::errs() << BB << "\n\n";

      // Iterate through each instruction in the basic block
      for (auto& I : BB) {
        Builder.SetInsertPoint(&I);
        // llvm::errs() << "Instruction: " << I << "\n";

        if (I.getType()->isIntegerTy(32)) {
          if (PackMap.count(&I) == 0) {
            ToPack.push_back(&I);
          }

          if (ToPack.size() == 2) {
            llvm::Value *Packed = doPack32(ToPack, Builder, Ctx);
            // for (auto *P : ToPack) {
            //   PackMap[P] = Packed;
            // }
            for (size_t i = 0; i < ToPack.size(); i++) {
              PackMap[ToPack[i]] = std::make_pair(Packed, i);
            }
            ToPack.clear();
          }
        }

        // if (llvm::PHINode *Phi = dyn_cast<llvm::PHINode>(&I)) {
        // }
        
        // Iterate through operands of the instruction
        // llvm::Value* V = &I;

        // V->mutateType(llvm::Type::getInt8Ty(Ctx));
        
        
        // if (!V->getType()->isIntegerTy()) {
        //   continue;
        // }

        // llvm::ConstantRange constant_range = LVI.getConstantRange(V, &I, true);
        // uint32_t bit_width = constant_range.getLower().getActiveBits();

        // llvm::errs() << "INSTR:              " << I << "\n"
        //              << "CONSTANT RANGE:     " << constant_range << "\n"
        //              << "NUM BITS:           " << bit_width
        //              << "\n\n";

        
      }

      if (ToPack.size() > 1) {
        llvm::Value* Packed = doPack32(ToPack, Builder, Ctx);
        for (size_t i = 0; i < ToPack.size(); i++) {
          PackMap[ToPack[i]] = std::make_pair(Packed, i);
        }
        // for (auto* Scalar : ToPack) {
        //   PackMap[Scalar] = Packed;
        // }
      }
    }

    for (auto &[Orig, Pair] : PackMap) {
      llvm::Value* Packed = Pair.first;
      size_t Index = Pair.second;

      // auto begin = Orig->users().begin();
      llvm::Value* Unpacked = doUnpack32(Packed, Index, Builder, Ctx);
      // Orig->replaceAllUsesWith(Unpacked);
      for (auto &use : Orig->uses()) {
        // user must come AFTER Packed!
        auto userInstr = dyn_cast<llvm::Instruction>(use.getUser());
        if (!userInstr) continue;

        if (auto *PackedInstr = dyn_cast<llvm::Instruction>(Packed)) {
          if (PackedInstr->getParent() == userInstr->getParent() && PackedInstr->comesBefore(userInstr)) {
            use.set(Unpacked);
          }
        }
      }
    }

    llvm::errs() << "\n\n" << F;

    return llvm::PreservedAnalyses::none();
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
