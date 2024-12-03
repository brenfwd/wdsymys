#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>

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
  Instruction* packedVal;
  size_t startBit;
  IntegerType* type;

  PackedVal(Instruction* pv, size_t s, IntegerType* t)
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
    std::vector<llvm::PHINode*> PHINodesToRewrite;
    std::vector<Instruction*> InstructionsToErase;

    for (auto& BB : F) {
      for (auto& I : BB) {
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
            if (I.getOpcode() == llvm::Instruction::ZExt || I.getOpcode() == llvm::Instruction::SExt) {
              if (BestType != I.getOperand(0)->getType()) {
                continue;
              }

              for (llvm::Use& Use : I.uses()) {
                if (auto* UserInst = dyn_cast<Instruction>(Use.getUser())) {
                  InstructionsToRewrite.push_back(UserInst);
                }
                if (auto* UserPHI = dyn_cast<llvm::PHINode>(Use.getUser())) {
                  PHINodesToRewrite.push_back(UserPHI);
                }
              }
              
              I.replaceAllUsesWith(I.getOperand(0));
              InstructionsToErase.push_back(&I);

              continue;
            }

            I.mutateType(BestType);
            InstructionsToRewrite.push_back(&I);

            for (llvm::Use& Use : I.uses()) {
              if (auto* UserInst = dyn_cast<Instruction>(Use.getUser())) {
                InstructionsToRewrite.push_back(UserInst);
              }
              if (auto* UserPHI = dyn_cast<llvm::PHINode>(Use.getUser())) {
                PHINodesToRewrite.push_back(UserPHI);
              }
            }
          }
        }
      }
    }

    // errs() << F << "\n\n";

    for (Instruction* I : InstructionsToErase) {
      I->eraseFromParent();
    }

    for (Instruction* UserInst : InstructionsToRewrite) {
      unsigned int numOps = UserInst->getNumOperands();
      if (numOps == 2) {
        // Upcast smaller op type to bigger to prevent any flows
        if (IntegerType* UserInstType =
                dyn_cast<IntegerType>(UserInst->getType())) {
          // Find largest type among operands and instruction type
          IntegerType* LargestOperandType = UserInstType;
          for (Value* Operand : UserInst->operands()) {
            IntegerType* OperandType = dyn_cast<IntegerType>(Operand->getType());

            if (!OperandType) {
              continue;
            }

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
            if (!isa<llvm::IntegerType>(Operand.get()->getType())) {
              continue;
            }

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
              // errs() << "sext " << *Operand.get()->getType() << " to " << *LargestOperandType << "\n";
              llvm::Value* Ext =
                  Builder.CreateSExt(Operand.get(), LargestOperandType, "_lvi_sext");
              Operand.set(Ext);
            }
          }

          // Truncate instruction result if possible
          if (UserInstType != LargestOperandType &&
              UserInst->getOpcode() != llvm::Instruction::ICmp) {
            Builder.SetInsertPoint(UserInst->getNextNode());
            llvm::Value* Trunc =
                Builder.CreateTrunc(UserInst, UserInstType, "_lvi_trunc");
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
            // errs() << "sext " << *UserInst->getOperand(0)->getType() << " to " << *FReturnType << "\n";
          llvm::Value* Ext =
              Builder.CreateSExt(UserInst->getOperand(0), FReturnType, "_lvi_sext");
          UserInst->setOperand(0, Ext);
        }
      }
    }

    InstructionsToErase.clear();    
    for (auto& BB : F) {
      for (auto& I : BB) {
        if (I.getOpcode() == llvm::Instruction::SExt) {
          if (I.getOperand(0)->getType() == I.getType()) {
            I.replaceAllUsesWith(I.getOperand(0));
            InstructionsToErase.push_back(&I);
          }
        }
      }
    }

    for (Instruction* I : InstructionsToErase) {
      I->eraseFromParent();
    }

    // errs() << F << "\n\n";

    return llvm::PreservedAnalyses::none();
  }
};

struct WDSYMYSPackingPass : public llvm::PassInfoMixin<WDSYMYSPackingPass> {
 private:
  llvm::Instruction* doPack(const llvm::SmallVector<PackedVal, 8>& ToPack,
                      llvm::IRBuilder<>& Builder, llvm::LLVMContext& Ctx) {
    llvm::Type* I64 = llvm::Type::getInt64Ty(Ctx);
    Builder.SetInsertPoint(
          static_cast<llvm::Instruction*>(ToPack[0].packedVal)->getNextNode());
    llvm::Instruction* PackedValue = dyn_cast<Instruction>(Builder.CreateZExt(ToPack[0].packedVal, I64, "ext"));

    for (size_t i = 1; i < ToPack.size(); i++) {
      unsigned sz = ToPack[i].type->getBitWidth();

      Builder.SetInsertPoint(
          static_cast<llvm::Instruction*>(ToPack[i].packedVal)->getNextNode());
      llvm::Value* Ext = Builder.CreateZExt(ToPack[i].packedVal, I64, "ext");
      llvm::Value* Shifted = Builder.CreateShl(
          Ext, llvm::ConstantInt::get(I64, ToPack[i].startBit), "shifted");
      PackedValue = dyn_cast<Instruction>(Builder.CreateOr(PackedValue, Shifted, "packed"));
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
    llvm::DependenceInfo& Dependencies = FAM.getResult<llvm::DependenceAnalysis>(F);

    for (auto& BB : F) {
      WDSYMYSPacking* ToPack = new WDSYMYSPacking();

      // llvm::errs() << "Basic Block: " << BB.getName() << "\n";
      // llvm::errs() << BB << "\n\n";

      // Iterate through each instruction in the basic block
      for (auto& I : BB) {
        if (I.getName().starts_with("_lvi_") && I.getOpcode() == llvm::Instruction::SExt) {
          continue;
        }
        
        // if (I.getOpcode() == llvm::Instruction::ZExt) {
        //   continue;
        // }

        // if (I.getOpcode() == llvm::Instruction::SExt) {
        //   continue;
        // }

        // if (I.getOpcode() == llvm::Instruction::Trunc) {
        //   continue;
        // }

        Builder.SetInsertPoint(&I);
        if (IntegerType* type = dyn_cast<IntegerType>(I.getType())) {
          if (!llvm::isa<llvm::PHINode>(&I)) {
            if (I.hasOneUse()) {
              if (auto* UserInstruction = dyn_cast<Instruction>(*I.user_begin())) {
                int distance = 0;
                bool foundFirst = false;

                for (auto &It : BB) {
                    if (&It == &I)
                        foundFirst = true;
                    else if (&It == UserInstruction) {
                        distance = foundFirst ? distance : -1;
                        break;
                    }

                    if (foundFirst)
                        distance++;
                }
                if (distance < 4) {
                  continue;
                }

              }
            }

            size_t sz = type->getBitWidth();

            if (ToPack->bits_remaining < sz) {
              llvm::Instruction* Packed = doPack(ToPack->to_pack, Builder, Ctx);

              size_t currPos = 0;
              for (size_t i = 0; i < ToPack->to_pack.size(); i++) {
                IntegerType* type = ToPack->to_pack[i].type;
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
    
      if (ToPack->to_pack.size() > 1) {
        llvm::Instruction* Packed = doPack(ToPack->to_pack, Builder, Ctx);

        size_t currPos = 0;
        for (size_t i = 0; i < ToPack->to_pack.size(); i++) {
          IntegerType* type = ToPack->to_pack[i].type;
          PackMap.try_emplace(ToPack->to_pack[i].packedVal, Packed, currPos,
                              type);

          size_t sz = type->getBitWidth();
          currPos += sz;
        }
        ToPack->clear();
      }
    }

    for (auto& [Orig, Pair] : PackMap) {
      llvm::Instruction* Packed = Pair.packedVal;
      size_t Index = Pair.startBit;
      IntegerType* type = Pair.type;

      // errs() << *Orig << " was packed and has "<< Orig->getNumUses() << " uses\n";

      std::vector<llvm::Use*> UsesToReplace;

      for (auto& use : Orig->uses()) {
        // user must come AFTER Packed!

        if (auto* UserInstr = dyn_cast<llvm::Instruction>(use.getUser())) {
          if ((Packed->getParent() == UserInstr->getParent() &&
               Packed->comesBefore(UserInstr)) || Packed->getParent() != UserInstr->getParent()) {
            UsesToReplace.push_back(&use);
          }
        }

        if (auto* UserPHINode = dyn_cast<llvm::PHINode>(use.getUser())) {
          llvm::BasicBlock* BBPredecessor = UserPHINode->getIncomingBlock(use);
          Builder.SetInsertPoint(BBPredecessor->getTerminator());
          llvm::Value* Unpacked = doUnpack(Packed, Index, type, Builder, Ctx);
          UserPHINode->setIncomingValueForBlock(BBPredecessor, Unpacked);
        }
      }

      for (auto* UseToReplace : UsesToReplace) {
        if (auto* UserInstr = dyn_cast<llvm::Instruction>(UseToReplace->getUser())) {
          if ((Packed->getParent() == UserInstr->getParent() &&
               Packed->comesBefore(UserInstr)) || Packed->getParent() != UserInstr->getParent()) {
            Builder.SetInsertPoint(UserInstr);
            llvm::Value* Unpacked = doUnpack(Packed, Index, type, Builder, Ctx);
            UseToReplace->set(Unpacked);
          }
        }
      }
    }

    errs() << "wdsymys done\n";

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
            PB.registerPipelineEarlySimplificationEPCallback(
              [&](llvm::ModulePassManager &MPM, auto) {
                MPM.addPass(createModuleToFunctionPassAdaptor(llvm::PromotePass()));
                // MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSLVIPass()));
                MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSPackingPass()));
                return true;
              });
          }};
}
