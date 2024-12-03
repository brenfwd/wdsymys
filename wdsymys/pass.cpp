#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

#include <vector>

namespace {

using llvm::Value, llvm::Type, llvm::Instruction, llvm::SmallVector,
    llvm::DenseMap, llvm::errs, llvm::IntegerType;

IntegerType* getBestIntegerType(size_t BitWidth, llvm::LLVMContext& Ctx) {
  return (BitWidth == 1)    ? Type::getInt1Ty(Ctx)
         : (BitWidth <= 8)  ? Type::getInt8Ty(Ctx)
         : (BitWidth <= 16) ? Type::getInt16Ty(Ctx)
         : (BitWidth <= 32) ? Type::getInt32Ty(Ctx)
                            : Type::getInt64Ty(Ctx);
}

// std::pairs do not bring me joy
struct PackedVal {
  Value* packedVal;
  size_t startBit;
  IntegerType* type;

  PackedVal(Value* pv, size_t s, IntegerType* t)
      : packedVal(pv), startBit(s), type(t) {}

  PackedVal() : packedVal(nullptr), startBit(0), type(nullptr) {}
};

struct WDSYMYSPacking {
  int bits_remaining;
  // Max size of 8 * 8 bit values for a 64 bit reg
  llvm::SmallVector<PackedVal, 8> to_pack;

  WDSYMYSPacking() : bits_remaining(64) {}

  void clear() {
    bits_remaining = 64;
    to_pack.clear();
  }
};

struct WDSYMYSLVIPass : public llvm::PassInfoMixin<WDSYMYSLVIPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Function& F,
                              llvm::FunctionAnalysisManager& FAM) {
    llvm::LLVMContext& Ctx = F.getContext();
    llvm::IRBuilder<> Builder(Ctx);
    llvm::LazyValueInfo& LVI = FAM.getResult<llvm::LazyValueAnalysis>(F);

    std::vector<Instruction*> InstructionsToRewrite;

    errs() << F << "\n\n";

    for (auto& BB : F) {
      for (auto& I : BB) {
        if (I.getOpcode() == llvm::Instruction::ZExt) {
          continue;
        }

        if (I.getOpcode() == llvm::Instruction::SExt) {
          continue;
        }

        if (I.getOpcode() == llvm::Instruction::Trunc) {
          continue;
        }

        if (IntegerType* type = dyn_cast<IntegerType>(I.getType())) {
          auto CR = LVI.getConstantRange(&I, &I, false);
          size_t BitWidth = CR.getUpper().getActiveBits();
          IntegerType* BestType = getBestIntegerType(BitWidth, Ctx);

          // errs() << "Instruction:   " << I << "\n";
          // errs() << "Range:         " << CR << "\n";
          // errs() << "Bits required: " << BitWidth << "\n";
          // errs() << "Best type:     " << *BestType << "\n";
          // errs() << "\n";

          if (BestType != I.getType()) {
            I.mutateType(BestType);
            InstructionsToRewrite.push_back(&I);

            for (llvm::Use& Use : I.uses()) {
              if (Instruction* UserInst = dyn_cast<Instruction>(Use.getUser())) {
                InstructionsToRewrite.push_back(UserInst);
              }
            }
          }
        }
      }
    }

    // errs() << F << "\n\n";

    for (Instruction* UserInst : InstructionsToRewrite) {
      unsigned int numOps = UserInst->getNumOperands();
      if (numOps == 2) {
        // Upcast smaller op type to bigger to prevent any flows
        if (IntegerType* UserInstType =
                dyn_cast<IntegerType>(UserInst->getType())) {
          // Find largest type among operands and instruction type
          IntegerType* LargestOperandType = UserInstType;
          for (Value* Operand : UserInst->operands()) {
            IntegerType* OperandType = cast<IntegerType>(Operand->getType());
            if (OperandType->getBitWidth() >
                    LargestOperandType->getBitWidth() &&
                !isa<llvm::Constant>(Operand)) {
              LargestOperandType = OperandType;
            }

            if (auto* ConstantOperand = dyn_cast<llvm::Constant>(Operand)) {
              size_t ConstantBitWidth =
                  ConstantOperand->getUniqueInteger().getActiveBits();
              if (ConstantBitWidth > LargestOperandType->getBitWidth()) {
                LargestOperandType = getBestIntegerType(ConstantBitWidth, Ctx);
              }
            }
          }
          // errs() << "Largest operand type of" << *UserInst << ": "
          //        << *LargestOperandType << "\n";
          // Upcast constants to match
          for (llvm::Use& Operand : UserInst->operands()) {
            if (auto* ConstantOperand = dyn_cast<llvm::Constant>(Operand)) {
              // errs() << "The largest type is: " << *LargestOperandType << "\n";
              llvm::Constant* NewConstant = llvm::ConstantInt::get(
                  LargestOperandType, ConstantOperand->getUniqueInteger().trunc(
                                          LargestOperandType->getBitWidth()));
              // errs() << "Changed constant to have type "
              //        << *NewConstant->getType() << ", should be "
              //        << *LargestOperandType << "\n";
              Operand.set(NewConstant);
            } else if (Operand.get()->getType() != LargestOperandType) {
              Builder.SetInsertPoint(UserInst);
              llvm::Value* Ext =
                  Builder.CreateSExt(Operand.get(), LargestOperandType, "sext");
              Operand.set(Ext);
            }
          }

          // Truncate instruction result if possible
          if (UserInstType != LargestOperandType &&
              UserInst->getOpcode() != llvm::Instruction::ICmp) {
            Builder.SetInsertPoint(UserInst->getNextNode());
            llvm::Value* Trunc =
                Builder.CreateTrunc(UserInst, UserInstType, "trunc");
            UserInst->mutateType(LargestOperandType);
            for (auto& use : UserInst->uses()) {
              if (use != Trunc) {
                use.set(Trunc);
              }
            }
          }
        }
        // else {
        // yeah idk, this is fucked
        // we put something in that we shouldn't have
        // errs() << *Use << "\n";
        // assert(false);
        // }
      } else if (UserInst->getOpcode() == Instruction::Ret) {
        Type* FReturnType = F.getReturnType();
        assert(FReturnType);
        if (FReturnType != UserInst->getOperand(0)->getType()) {
          Builder.SetInsertPoint(UserInst);
          llvm::Value* Ext =
              Builder.CreateSExt(UserInst->getOperand(0), FReturnType, "sext");
          UserInst->setOperand(0, Ext);
        }
      }
    }

    return llvm::PreservedAnalyses::none();
  }
};

struct WDSYMYSPackingPass : public llvm::PassInfoMixin<WDSYMYSPackingPass> {
 private:
  llvm::Value* doPack(const llvm::SmallVector<PackedVal, 8>& ToPack,
                      llvm::IRBuilder<>& Builder, llvm::LLVMContext& Ctx) {
    llvm::Type* I64 = llvm::Type::getInt64Ty(Ctx);
    llvm::Value* PackedValue = llvm::ConstantInt::get(I64, 0);

    for (size_t i = 0; i < ToPack.size(); i++) {
      unsigned sz = ToPack[i].type->getBitWidth();

      Builder.SetInsertPoint(
          static_cast<llvm::Instruction*>(ToPack[i].packedVal)->getNextNode());
      llvm::Value* Ext = Builder.CreateZExt(ToPack[i].packedVal, I64, "ext");
      llvm::Value* Shifted = Builder.CreateShl(
          Ext, llvm::ConstantInt::get(I64, ToPack[i].startBit), "shifted");
      PackedValue = Builder.CreateOr(PackedValue, Shifted, "packed");
    }
    return PackedValue;
  }

  llvm::Value* doUnpack(llvm::Value* Packed, size_t Index, IntegerType* type,
                        llvm::IRBuilder<>& Builder,
                        llvm::LLVMContext& Context) {
    llvm::Type* I64 = llvm::Type::getInt64Ty(Context);

    llvm::Value* Shifted = Builder.CreateLShr(
        Packed, llvm::ConstantInt::get(I64, Index), "unpack_shifted");
    return Builder.CreateTrunc(Shifted, type, "unpacked");
  }

 public:
  llvm::PreservedAnalyses run(llvm::Function& F,
                              llvm::FunctionAnalysisManager& FAM) {
    llvm::LLVMContext& Ctx = F.getContext();
    llvm::IRBuilder<> Builder(Ctx);
    llvm::DenseMap<llvm::Value*, PackedVal> PackMap;
    errs() << "skibidi called pack\n";
    llvm::errs() << "\n\n" << F;

    for (auto& BB : F) {
      WDSYMYSPacking* ToPack = new WDSYMYSPacking();

      // llvm::errs() << "Basic Block: " << BB.getName() << "\n";
      // llvm::errs() << BB << "\n\n";

      // Iterate through each instruction in the basic block
      errs() << "skibidi 1\n";
      for (auto& I : BB) {
        Builder.SetInsertPoint(&I);
        // llvm::errs() << "Instruction: " << I << "\n";
        if (IntegerType* type = dyn_cast<IntegerType>(I.getType())) {
          if (!llvm::isa<llvm::PHINode>(&I)) {
            size_t sz = type->getBitWidth();

            if (ToPack->bits_remaining < sz) {
              llvm::Value* Packed = doPack(ToPack->to_pack, Builder, Ctx);

              size_t currPos = 0;
              for (size_t i = 0; i < ToPack->to_pack.size(); i++) {
                IntegerType* type =
                    static_cast<IntegerType*>(ToPack->to_pack[i].type);
                PackMap.try_emplace(ToPack->to_pack[i].packedVal, Packed,
                                    currPos, type);

                size_t packed_value_sz = type->getBitWidth();
                currPos += packed_value_sz;
              }
              ToPack->clear();
            }
            ToPack->to_pack.emplace_back(&I, 64 - ToPack->bits_remaining,
                                          type);
            ToPack->bits_remaining -= sz;
          }
        }
      }
      
      errs() << "skibidi 2\n";

      if (ToPack->to_pack.size() > 1) {
        llvm::Value* Packed = doPack(ToPack->to_pack, Builder, Ctx);

        size_t currPos = 0;
        for (size_t i = 0; i < ToPack->to_pack.size(); i++) {
          IntegerType* type =
              static_cast<IntegerType*>(ToPack->to_pack[i].type);
          PackMap.try_emplace(ToPack->to_pack[i].packedVal, Packed, currPos,
                              type);
          PackMap[ToPack->to_pack[i].packedVal] =
              PackedVal(Packed, currPos, type);

          size_t sz = type->getBitWidth();
          currPos += sz;
        }
        ToPack->clear();
      }
    }

    for (auto& [Orig, Pair] : PackMap) {
      llvm::Value* Packed = Pair.packedVal;
      size_t Index = Pair.startBit;
      IntegerType* type = Pair.type;

      for (auto& use : Orig->uses()) {
        // user must come AFTER Packed!
        auto userInstr = dyn_cast<llvm::Instruction>(use.getUser());
        if (!userInstr)
          continue;

        if (auto* PackedInstr = dyn_cast<llvm::Instruction>(Packed)) {
          if (PackedInstr->getParent() == userInstr->getParent() &&
              PackedInstr->comesBefore(userInstr)) {
            Builder.SetInsertPoint(userInstr);
            llvm::Value* Unpacked = doUnpack(Packed, Index, type, Builder, Ctx);
            use.set(Unpacked);
          }
        }
      }
    }

    errs() << "skibidi done\n";
    llvm::errs() << "\n\n" << F;
    return llvm::PreservedAnalyses::none();
  }
};

}  // namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "WDSYMYS", "v0.1",
          [](llvm::PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager& FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "wdsymys-lvi") {
                    FPM.addPass(WDSYMYSLVIPass());
                    return true;
                  }
                  if (Name == "wdsymys-packing") {
                    FPM.addPass(WDSYMYSPackingPass());
                    return true;
                  }
                  return false;
                });
          }};
}
