#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Analysis/LazyValueInfo.h>
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

Instruction* findInsertionPointAfterPHIs(Instruction* Inst) {
  Instruction* succ = Inst->getNextNode();

  while (llvm::isa<llvm::PHINode>(succ)) {
    succ = succ->getNextNode();
  }

  if (!succ) {
    succ = Inst->getParent()->getTerminator();
  }

  return succ;
}

struct PackedVal {
  Instruction* packedVal;
  size_t startBit;
  IntegerType* IntType;

  PackedVal(Instruction* pv, size_t s, IntegerType* t)
      : packedVal(pv), startBit(s), IntType(t) {}

  PackedVal() : packedVal(nullptr), startBit(0), IntType(nullptr) {}
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
    std::vector<Instruction*> InstructionsToErase;

    llvm::DenseMap<llvm::Value*, llvm::IntegerType*> OriginalValueType;
    errs() << "====:(){:|:&}:==== " << F.getName() << " ORIGINAL IR ";
    errs() << "====:(){:|:&}:====\n";
    errs() << F << "\n\n";
    errs() << "====:(){:|:&}:==== " << F.getName() << " REWRITE LOGS ";
    errs() << "====:(){:|:&}:====\n";

    for (auto& BB : F) {
      for (auto& I : BB) {
        if (I.getOpcode() == llvm::Instruction::Trunc) {
          continue;
        }

        if (IntegerType* IntType = dyn_cast<IntegerType>(I.getType())) {
          auto CR = LVI.getConstantRange(&I, &I, false);
          size_t BitWidth = CR.getUpper().getActiveBits();
          IntegerType* BestType = getBestIntegerType(BitWidth, Ctx);

          llvm::DenseMap<llvm::Use*, llvm::Value*> UsesToReplace;

          if (BestType != I.getType()) {
            if (I.getOpcode() == llvm::Instruction::ZExt ||
                I.getOpcode() == llvm::Instruction::SExt) {
              if (BestType != I.getOperand(0)->getType()) {
                continue;
              }

              for (llvm::Use& Use : I.uses()) {
                if (Instruction* UserInst =
                        dyn_cast<Instruction>(Use.getUser())) {
                  InstructionsToRewrite.push_back(UserInst);
                }
              }
              //UsesToReplace[I.get()] = dyn_cast<IntegerType>(I.getType());
              OriginalValueType[I.getOperand(0)] = IntType;

              errs() << "I'm replacing " << I << " with " << *I.getOperand(0) << "\n";

              I.replaceAllUsesWith(I.getOperand(0));
              InstructionsToErase.push_back(&I);

              continue;
            }

            OriginalValueType[&I] = IntType;
            I.mutateType(BestType);
            InstructionsToRewrite.push_back(&I);
            errs() << "rewriting " << I << " because we mutated its type\n";

            for (llvm::Use& Use : I.uses()) {
              if (Instruction* UserInst =
                      dyn_cast<Instruction>(Use.getUser())) {
                InstructionsToRewrite.push_back(UserInst);
                errs() << "rewriting " << *UserInst << " because it uses a value we mutated\n";
                errs() << "i belive it ";
                if (llvm::isa<llvm::CallBase>(UserInst)) {
                  errs() << "is ";
                } else {
                  errs() << "is not ";
                }
                errs() << "a function\n";
              }
            }
          }
        }
      }
    }

    // for (Instruction* I : InstructionsToErase) {
    //   I->eraseFromParent();
    // }

    for (Instruction* UserInst : InstructionsToRewrite) {
      llvm::DenseMap<llvm::Use*, llvm::Value*> UsesToReplace;
      unsigned int numOps = UserInst->getNumOperands();

      errs() << "rewriting " << *UserInst << " now\n";

      if (llvm::isa<llvm::CallBase>(UserInst)) {
        errs() << "im a function: " << *UserInst << "\n";
        for (llvm::Use& FuncArg : UserInst->operands()) {
          errs() << "other ptr: " << FuncArg << "\n";
          errs() << "Im looking at the function arg " << FuncArg.get() << ": " << *FuncArg << "\n";
          if (OriginalValueType.count(FuncArg.get()) == 0) {
            continue;
          }

          llvm::IntegerType* OriginalType = OriginalValueType[FuncArg.get()];
          llvm::IntegerType* CurrentType = dyn_cast<IntegerType>(FuncArg.get()->getType());
          
          errs() << "The original type was: " << *OriginalType << "\n";
          errs() << "We changed it to: " << *CurrentType << "\n";
          Builder.SetInsertPoint(UserInst);
          if (OriginalType->getBitWidth() > CurrentType->getBitWidth()) {
            errs() << "I am correcting the change by sexting\n";
            llvm::Value* Ext = Builder.CreateSExt(FuncArg.get(), OriginalType, "_lvi_sext_c");
            UsesToReplace[&FuncArg] = Ext;
          } else if (OriginalType->getBitWidth() < CurrentType->getBitWidth()) {
            errs() << "I am correcting the change by truncing\n";
            llvm::Value* Trunc = Builder.CreateTrunc(FuncArg.get(), OriginalType, "_lvi_trunc_a");
            UsesToReplace[&FuncArg] = Trunc;
          }
        }
      } else if (isa<llvm::BinaryOperator>(UserInst) 
                || UserInst->getOpcode() == Instruction::ICmp
                || UserInst->getOpcode() == Instruction::Select
                || UserInst->getOpcode() == Instruction::PHI) {
        // Upcast smaller op IntType to bigger to prevent any flows
        if (IntegerType* UserInstType =
                dyn_cast<IntegerType>(UserInst->getType())) {
          // Find largest IntType among operands and instruction IntType
          IntegerType* LargestOperandType = UserInstType;
          for (Value* Operand : UserInst->operands()) {
            IntegerType* OperandType =
                dyn_cast<IntegerType>(Operand->getType());

            if (!OperandType) {
              continue;
            }
            errs() << *Operand << " is an " << *OperandType << "\n";

            if (OperandType->getBitWidth() >
                    LargestOperandType->getBitWidth() &&
                !isa<llvm::Constant>(Operand)) {
              errs() << "Changing largest type to " << *OperandType << ", was " << *LargestOperandType << "\n";
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

          // Upcast operands to match
          for (llvm::Use& Operand : UserInst->operands()) {
            if (!isa<llvm::IntegerType>(Operand.get()->getType())) {
              continue;
            }

            // first argument of select is always a bool
            if (UserInst->getOpcode() == Instruction::Select && Operand.getOperandNo() == 0) {
              continue;
            }

            if (auto* ConstantOperand = dyn_cast<llvm::Constant>(Operand)) {
              llvm::Constant* NewConstant = llvm::ConstantInt::get(
                  LargestOperandType, ConstantOperand->getUniqueInteger().trunc(
                                          LargestOperandType->getBitWidth()));
              UsesToReplace[&Operand] = NewConstant;
            } else if (Operand.get()->getType() != LargestOperandType) {
              Instruction* InsertPoint = UserInst;

              if (auto* UserPHI = dyn_cast<llvm::PHINode>(UserInst)) {
                InsertPoint =
                    UserPHI->getIncomingBlock(Operand)->getTerminator();
              }

              Builder.SetInsertPoint(InsertPoint);
              errs() << "creating sext from " << *Operand.get() << ", " << *Operand.get()->getType() << " to type " << *LargestOperandType << "\n";
              llvm::Value* Ext = Builder.CreateSExt(
                  Operand.get(), LargestOperandType, "_lvi_sext_a");
              UsesToReplace[&Operand] = Ext;
            }
          }

          // Truncate instruction result if possible
          if (UserInstType != LargestOperandType &&
              UserInst->getOpcode() != llvm::Instruction::ICmp && 
              UserInst->getOpcode() != llvm::Instruction::Select) {
            Builder.SetInsertPoint(findInsertionPointAfterPHIs(UserInst));

            UserInst->mutateType(LargestOperandType);
            // errs() << "mutating type of " << *UserInst << " from " << *UserInst->getType() << " to " << *LargestOperandType << "\n";
            errs() << "creating trunc from " << *UserInst << ", " << *UserInst->getType() << " to type " << *UserInstType << "\n";
            llvm::Value* Trunc =
                Builder.CreateTrunc(UserInst, UserInstType, "_lvi_trunc_b");
            OriginalValueType[Trunc] = dyn_cast<IntegerType>(UserInst->getType());
            for (auto& use : UserInst->uses()) {
              if (use.getUser() != Trunc) {
                UsesToReplace[&use] = Trunc;
              }
            }
          }
        }
      } else if (UserInst->getOpcode() == Instruction::Ret) {
        Type* FReturnType = F.getReturnType();
        assert(FReturnType);
        if (FReturnType != UserInst->getOperand(0)->getType()) {
          Builder.SetInsertPoint(UserInst);
          llvm::Value* Ext = Builder.CreateSExt(UserInst->getOperand(0),
                                                FReturnType, "_lvi_sext_b");
          UserInst->setOperand(0, Ext);
        }
      }

      for (auto& [Use, NewValue] : UsesToReplace) {
        errs() << "Setting " << **Use << " to " << *NewValue << "\n";
        Use->set(NewValue);
      }
    }

    errs() << "====:(){ :|:& };:&==== " << F.getName() << " FINAL IR ";
    errs() << "====:(){:|:&}:====\n";
    errs() << F << "\n\n";
    
    InstructionsToErase.clear();
    for (auto& BB : F) {
      for (auto& I : BB) {
        if (I.getOpcode() == llvm::Instruction::SExt || I.getOpcode() == llvm::Instruction::Trunc) {
          if (I.getOperand(0)->getType() == I.getType()) {
            errs() << "I'm fucking with a sext or a trunc: " << I << "\n";
            I.replaceAllUsesWith(I.getOperand(0));
            InstructionsToErase.push_back(&I);
          }
        }
      }
    }

    for (Instruction* I : InstructionsToErase) {
      I->eraseFromParent();
    }

    return llvm::PreservedAnalyses::none();
  }
};

struct WDSYMYSPackingPass : public llvm::PassInfoMixin<WDSYMYSPackingPass> {
 private:
  llvm::Instruction* doPack(const llvm::SmallVector<PackedVal, 8>& ToPack,
                            llvm::IRBuilder<>& Builder,
                            llvm::LLVMContext& Ctx) {
    llvm::Type* I64 = llvm::Type::getInt64Ty(Ctx);
    
    auto insertPoint = ToPack[0].packedVal->getNextNode();
    assert(insertPoint);
    Builder.SetInsertPoint(insertPoint);
    
    llvm::Instruction* PackedValue = dyn_cast<Instruction>(
        Builder.CreateZExt(ToPack[0].packedVal, I64, "ext"));

    for (size_t i = 1; i < ToPack.size(); i++) {
      unsigned sz = ToPack[i].IntType->getBitWidth();

      Builder.SetInsertPoint(
          dyn_cast<Instruction>(ToPack[i].packedVal)->getNextNode());
      llvm::Value* Ext = Builder.CreateZExt(ToPack[i].packedVal, I64, "ext");
      llvm::Value* Shifted = Builder.CreateShl(
          Ext, llvm::ConstantInt::get(I64, ToPack[i].startBit), "shifted");
      PackedValue = dyn_cast<Instruction>(
          Builder.CreateOr(PackedValue, Shifted, "packed"));
    }
    return PackedValue;
  }

  llvm::Value* doUnpack(llvm::Value* Packed, size_t Index, IntegerType* IntType,
                        llvm::IRBuilder<>& Builder,
                        llvm::LLVMContext& Context) {
    llvm::Type* I64 = llvm::Type::getInt64Ty(Context);

    llvm::Value* Shifted = Builder.CreateLShr(
        Packed, llvm::ConstantInt::get(I64, Index), "unpack_shifted");
    return Builder.CreateTrunc(Shifted, IntType, "unpacked");
  }

 public:
  llvm::PreservedAnalyses run(llvm::Function& F,
                              llvm::FunctionAnalysisManager& FAM) {
    llvm::LLVMContext& Ctx = F.getContext();
    llvm::IRBuilder<> Builder(Ctx);
    llvm::DenseMap<llvm::Value*, PackedVal> PackMap;
    llvm::DependenceInfo& Dependencies =
        FAM.getResult<llvm::DependenceAnalysis>(F);

    for (auto& BB : F) {
      WDSYMYSPacking* ToPack = new WDSYMYSPacking();

      // Iterate through each instruction in the basic block
      for (auto& I : BB) {
        if (I.getName().starts_with("_lvi_") &&
            I.getOpcode() == llvm::Instruction::SExt) {
          continue;
        }

        Builder.SetInsertPoint(&I);
        if (IntegerType* IntType = dyn_cast<IntegerType>(I.getType())) {
          if (!llvm::isa<llvm::PHINode>(&I)) {
            if (I.hasOneUse()) {
              if (auto* UserInstruction =
                      dyn_cast<Instruction>(*I.user_begin())) {
                int distance = 0;
                bool foundFirst = false;

                for (auto& It : BB) {
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

            size_t sz = IntType->getBitWidth();

            if (ToPack->bits_remaining < sz) {
              llvm::Instruction* Packed = doPack(ToPack->to_pack, Builder, Ctx);

              size_t currPos = 0;
              for (size_t i = 0; i < ToPack->to_pack.size(); i++) {
                IntegerType* IntType = ToPack->to_pack[i].IntType;
                PackMap.try_emplace(ToPack->to_pack[i].packedVal, Packed,
                                    currPos, IntType);

                size_t packed_value_sz = IntType->getBitWidth();
                currPos += packed_value_sz;
              }
              ToPack->clear();
            }
            ToPack->to_pack.emplace_back(&I, 64 - ToPack->bits_remaining, IntType);
            ToPack->bits_remaining -= sz;
          }
        }
      }

      if (ToPack->to_pack.size() > 1) {
        llvm::Instruction* Packed = doPack(ToPack->to_pack, Builder, Ctx);

        size_t currPos = 0;
        for (size_t i = 0; i < ToPack->to_pack.size(); i++) {
          IntegerType* IntType = ToPack->to_pack[i].IntType;
          PackMap.try_emplace(ToPack->to_pack[i].packedVal, Packed, currPos,
                              IntType);

          size_t sz = IntType->getBitWidth();
          currPos += sz;
        }
        ToPack->clear();
      }
    }

    for (auto& [Orig, Pair] : PackMap) {
      llvm::Instruction* Packed = Pair.packedVal;
      size_t Index = Pair.startBit;
      IntegerType* IntType = Pair.IntType;


      std::vector<llvm::Use*> UsesToReplace;

      for (auto& use : Orig->uses()) {
        // user must come AFTER Packed!

        if (auto* UserPHINode = dyn_cast<llvm::PHINode>(use.getUser())) {
          llvm::BasicBlock* BBPredecessor = UserPHINode->getIncomingBlock(use);
          Builder.SetInsertPoint(BBPredecessor->getTerminator());
          llvm::Value* Unpacked = doUnpack(Packed, Index, IntType, Builder, Ctx);
          UserPHINode->setIncomingValueForBlock(BBPredecessor, Unpacked);
        } else if (auto* UserInstr =
                       dyn_cast<llvm::Instruction>(use.getUser())) {
          if ((Packed->getParent() == UserInstr->getParent() &&
               Packed->comesBefore(UserInstr)) ||
              Packed->getParent() != UserInstr->getParent()) {
            UsesToReplace.push_back(&use);
          }
        }
      }

      for (auto* use : UsesToReplace) {
        Builder.SetInsertPoint(cast<llvm::Instruction>(use->getUser()));
        llvm::Value* Unpacked = doUnpack(Packed, Index, IntType, Builder, Ctx);
        use->set(Unpacked);
      }
    }


    return llvm::PreservedAnalyses::none();
  }
};

}  // namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "WDSYMYS", "v0.1", [](llvm::PassBuilder& PB) {
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
        PB.registerOptimizerLastEPCallback([&](llvm::ModulePassManager& MPM,
                                               auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(llvm::PromotePass()));
          MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSLVIPass()));
          // MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSPackingPass()));
          return true;
        });

        // PB.registerPipelineEarlySimplificationEPCallback(
        //     [&](llvm::ModulePassManager& MPM, auto) {
        //       MPM.addPass(createModuleToFunctionPassAdaptor(
        //           llvm::PromotePass()));
        //       MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSLVIPass()));
        //       MPM.addPass(createModuleToFunctionPassAdaptor(
        //           WDSYMYSPackingPass()));
        //       return true;
        //     });
      }};
}
