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

#include <vector>
#include <unordered_set>

// #define DEBUG
constexpr bool DEBUG = false;

class Debug {
  public:
  template <typename T>
    auto& operator<<(const T& t) {
      if constexpr (DEBUG) {
        llvm::errs() << t;
      }
      return *this;
    }
};


Debug debug;


namespace {

using llvm::Value, llvm::Type, llvm::Instruction, llvm::SmallVector,
    llvm::DenseMap, llvm::errs, llvm::IntegerType;

IntegerType* getBestIntegerType(size_t BitWidth, llvm::LLVMContext& Ctx) {
  return (BitWidth == 1)    ? Type::getInt1Ty(Ctx)
         : (BitWidth <= 8)  ? Type::getInt8Ty(Ctx)
         : (BitWidth <= 16) ? Type::getInt16Ty(Ctx)
         : (BitWidth <= 32) ? Type::getInt32Ty(Ctx)
         : (BitWidth <= 64) ? Type::getInt64Ty(Ctx)
                            : Type::getInt128Ty(Ctx);
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
    std::unordered_set<Instruction*> InstructionsToErase;
    
    
    debug << "====:(){:|:&}:==== " << F.getName() << " ORIGINAL IR ";
    debug << "====:(){:|:&}:====\n";
    debug << F << "\n\n";
    debug << "====:(){:|:&}:==== " << F.getName() << " REWRITE LOGS ";
    debug << "====:(){:|:&}:====\n";

    for (auto& BB : F) {
      for (auto& I : BB) {
        if (I.getOpcode() == llvm::Instruction::Trunc || I.getOpcode() == llvm::Instruction::BitCast) {
          continue;
        }

        if (auto* IntType = dyn_cast<IntegerType>(I.getType())) {
          debug << "Considering instruction " << I << "\n";
          auto CR = LVI.getConstantRange(&I, &I, true);
          // size_t BitWidth = CR.getUpper().getActiveBits();
          size_t BitWidth = CR.getBitWidth();
          debug << I << " has range " << CR << " and bit width " << BitWidth << "\n";
          // debug << "ConstantRange says bit width is " << CR.getBitWidth() << "\n";
          IntegerType* BestType = getBestIntegerType(BitWidth, Ctx);

          if (BestType != I.getType()) {
            if (I.getOpcode() == llvm::Instruction::Call) {
              continue;
            }

            if (I.getOpcode() == llvm::Instruction::ZExt ||
                I.getOpcode() == llvm::Instruction::SExt) {
              if (BestType != I.getOperand(0)->getType()) {
                continue;
              }

              for (llvm::Use& Use : I.uses()) {
                if (Instruction* UserInst = dyn_cast<Instruction>(Use.getUser())) {
                  InstructionsToRewrite.push_back(UserInst);
                }
              }

              debug << "I'm replacing " << I << " with " << *I.getOperand(0) << "\n";

              I.replaceAllUsesWith(I.getOperand(0));
              InstructionsToErase.insert(&I);

              continue;
            }

            I.mutateType(BestType);
            InstructionsToRewrite.push_back(&I);
            debug << "rewriting " << I << " @ << " << &I << " because we mutated its type, original type is " << *IntType << "\n";

            for (llvm::Use& Use : I.uses()) {
              if (Instruction* UserInst =
                      dyn_cast<Instruction>(Use.getUser())) {
                InstructionsToRewrite.push_back(UserInst);
                debug << "rewriting " << *UserInst << " because it uses a value we mutated\n";
                debug << "i belive it ";
                if (llvm::isa<llvm::CallBase>(UserInst)) {
                  debug << "is ";
                } else {
                  debug << "is not ";
                }
                debug << "a function\n";
              }
            }
          }
        }
      }
    }

    // for (Instruction* I : InstructionsToErase) {
    //   I->eraseFromParent();
    // }
    // InstructionsToErase.clear();

    for (Instruction* UserInst : InstructionsToRewrite) {
      llvm::DenseMap<llvm::Use*, llvm::Value*> UsesToReplace;
      unsigned int numOps = UserInst->getNumOperands();

      debug << "rewriting " << *UserInst << " now\n";

      if (auto* FunctionCall = dyn_cast<llvm::CallBase>(UserInst)) {
        debug << "im a function call: " << *FunctionCall << "\n";
        debug << "call type or smth??: " << *FunctionCall->getFunctionType() << "\n";
        llvm::FunctionType* CalledFunctionType = FunctionCall->getFunctionType();
        for (size_t i = 0; i < CalledFunctionType->getNumParams(); i++) {
          llvm::Use& FuncArg = UserInst->getOperandUse(i);
          debug << "other ptr: " << FuncArg << "\n";
          debug << "Im looking at the function arg " << FuncArg.get() << ": " << *FuncArg << "\n";
          auto* OriginalType = dyn_cast<IntegerType>(CalledFunctionType->getParamType(FuncArg.getOperandNo()));
          auto* CurrentType = dyn_cast<IntegerType>(FuncArg.get()->getType());

          // if they're not ints we didn't mess with them
          if (!OriginalType || !CurrentType) {
            debug << "types not int, ignoring\n";
            continue;
          }

          debug << "Arg number is: " << FuncArg.getOperandNo() << "\n";

          debug << "The original type was: " << *OriginalType << "\n";
          debug << "We changed it to: " << *CurrentType << "\n";
          Builder.SetInsertPoint(FunctionCall);
          if (OriginalType->getBitWidth() > CurrentType->getBitWidth()) {
            debug << "I am correcting the change by sexting\n";
            llvm::Value* Ext = Builder.CreateSExt(FuncArg.get(), OriginalType, "_lvi_sext_c");
            UsesToReplace[&FuncArg] = Ext;
          } else if (OriginalType->getBitWidth() < CurrentType->getBitWidth()) {
            debug << "I am correcting the change by truncing\n";
            llvm::Value* Trunc = Builder.CreateTrunc(FuncArg.get(), OriginalType, "_lvi_trunc_a");
            UsesToReplace[&FuncArg] = Trunc;
          }
        }
      } else if (isa<llvm::BinaryOperator>(UserInst) ||
                 UserInst->getOpcode() == Instruction::ICmp ||
                 UserInst->getOpcode() == Instruction::Select ||
                 UserInst->getOpcode() == Instruction::PHI) {
        // Upcast smaller op IntType to bigger to prevent any flows
        if (IntegerType* UserInstType =
                dyn_cast<IntegerType>(UserInst->getType())) {
          // Find largest IntType among operands and instruction IntType
          IntegerType* LargestOperandType = UserInstType;
          for (llvm::Use& Operand : UserInst->operands()) {
            IntegerType* OperandType =
                dyn_cast<IntegerType>(Operand->getType());

            if (!OperandType) {
              continue;
            }
            debug << *Operand << " is an " << *OperandType << "\n";

            if (UserInst->getOpcode() == Instruction::Select && Operand.getOperandNo() == 0) {
              continue;
            }

            if (OperandType->getBitWidth() >
                    LargestOperandType->getBitWidth() &&
                !isa<llvm::Constant>(&*Operand)) {
              debug << "Changing largest type to " << *OperandType << ", was "
                     << *LargestOperandType << "\n";
              LargestOperandType = OperandType;
            }

            if (auto* ConstantOperand = dyn_cast<llvm::Constant>(&*Operand)) {
              if (isa<llvm::UndefValue>(ConstantOperand) || isa<llvm::PoisonValue>(ConstantOperand) || !isa<llvm::IntegerType>(ConstantOperand->getType()) || isa<llvm::ConstantExpr>(&*Operand)) {
                continue;
              }

              debug << "retyping constant " << *ConstantOperand << "\n";

              size_t ConstantBitWidth =
                  ConstantOperand->getUniqueInteger().getActiveBits();
              if (ConstantBitWidth > LargestOperandType->getBitWidth()) {
                LargestOperandType = getBestIntegerType(ConstantBitWidth, Ctx);
              }
            }
          }

          // Upcast operands to match
          for (llvm::Use& Operand : UserInst->operands()) {
            if (!isa<IntegerType>(Operand.get()->getType())) {
              continue;
            }

            // first argument of select is always a bool
            if (UserInst->getOpcode() == Instruction::Select &&
                Operand.getOperandNo() == 0) {
              continue;
            }

            if (auto* ConstantOperand = dyn_cast<llvm::Constant>(Operand); ConstantOperand && !isa<llvm::ConstantExpr>(Operand)) {
              if (isa<llvm::UndefValue>(ConstantOperand)) {
                llvm::Constant* NewConstant = llvm::UndefValue::get(LargestOperandType);
                UsesToReplace[&Operand] = NewConstant;
              } else if (isa<llvm::PoisonValue>(ConstantOperand)) {
                llvm::Constant* NewConstant = llvm::PoisonValue::get(LargestOperandType);
                UsesToReplace[&Operand] = NewConstant;
              } else {
                llvm::Constant* NewConstant = llvm::ConstantInt::get(
                    LargestOperandType, ConstantOperand->getUniqueInteger().trunc(
                                            LargestOperandType->getBitWidth()));
                UsesToReplace[&Operand] = NewConstant;
              }
            } else if (Operand.get()->getType() != LargestOperandType) {
              Instruction* InsertPoint = UserInst;

              if (auto* UserPHI = dyn_cast<llvm::PHINode>(UserInst)) {
                InsertPoint =
                    UserPHI->getIncomingBlock(Operand)->getTerminator();
              }

              auto* OperandType = dyn_cast<llvm::IntegerType>(Operand->getType());
              assert(OperandType);

              Builder.SetInsertPoint(InsertPoint);
              if (OperandType->getBitWidth() > LargestOperandType->getBitWidth()) {
                debug << "creating trunc from " << *Operand.get() << ", "
                       << *Operand.get()->getType() << " to type "
                       << *LargestOperandType << "\n";
                llvm::Value* Trunc =
                    Builder.CreateTrunc(&*Operand, LargestOperandType, "_lvi_trunc_g");
                UsesToReplace[&Operand] = Trunc;
              } else {
                debug << "creating sext from " << *Operand.get() << ", "
                       << *Operand.get()->getType() << " to type "
                       << *LargestOperandType << "\n";
                llvm::Value* Ext = Builder.CreateSExt(Operand,
                                                      LargestOperandType, "_lvi_sext_a");
                UsesToReplace[&Operand] = Ext;
              }
            }
          }

          // Truncate instruction result if possible
          if (UserInstType != LargestOperandType &&
              UserInst->getOpcode() != Instruction::ICmp) {
            Builder.SetInsertPoint(findInsertionPointAfterPHIs(UserInst));
            UserInst->mutateType(LargestOperandType);
            debug << "mutating type of " << *UserInst << " from " << *UserInst->getType() << " to " << *LargestOperandType << "\n";

            debug << "creating trunc from " << *UserInst << ", "
                   << *UserInst->getType() << " to type " << *UserInstType
                   << "\n";
            llvm::Value* Trunc =
                Builder.CreateTrunc(UserInst, UserInstType, "_lvi_trunc_b");
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
      } else if (UserInst->getOpcode() == Instruction::Trunc) {
        // changed the type of the operand to an existing trunc, we may have
        // made it too small
        auto* UserInstType = dyn_cast<IntegerType>(UserInst->getType());
        assert(UserInstType);

        auto* Operand = dyn_cast<Instruction>(UserInst->getOperand(0));
        assert(Operand);

        auto* OperandType = dyn_cast<IntegerType>(Operand->getType());
        assert(OperandType);

        // If the Trunc will succeed, it can live
        if (UserInstType->getBitWidth() < OperandType->getBitWidth()) {
          continue;
        }

        // If the Trunc is unnecessary, just remove it
        if (UserInstType == OperandType) {
          UserInst->replaceAllUsesWith(Operand);
          InstructionsToErase.insert(UserInst);
        }

        debug << "Removing Trunc " << *UserInst << " because " << *UserInstType << " is too large for rewritten operand with type" << *OperandType << "\n";

        // There was some reason to shrink this to the original type, so replace it with a SExt
        Builder.SetInsertPoint(UserInst);
        llvm::Value* Ext = Builder.CreateSExt(UserInst->getOperand(0),
                                              UserInstType, "_lvi_sext_d");
        UserInst->replaceAllUsesWith(Ext);
        InstructionsToErase.insert(UserInst);
      } else if (UserInst->getOpcode() == Instruction::SExt || UserInst->getOpcode() == Instruction::ZExt) {
        // changed the type of the operand to an existing extension, we may have
        // made it too large
        auto* UserInstType = dyn_cast<IntegerType>(UserInst->getType());
        assert(UserInstType);

        auto* Operand = dyn_cast<Instruction>(UserInst->getOperand(0));
        assert(Operand);

        auto* OperandType = dyn_cast<IntegerType>(Operand->getType());
        assert(OperandType);

        // If the ext will succeed, it can live
        if (UserInstType->getBitWidth() > OperandType->getBitWidth()) {
          continue;
        }

        // If the ext is unnecessary, just remove it
        if (UserInstType == OperandType) {
          UserInst->replaceAllUsesWith(Operand);
          InstructionsToErase.insert(UserInst);
        }

        debug << "Removing Trunc " << *UserInst << " because " << *UserInstType << " is too large for rewritten operand with type" << *OperandType << "\n";

        // There was some reason to extend this to the original type, so replace it with a Trunc
        Builder.SetInsertPoint(UserInst);
        llvm::Value* Trunc = Builder.CreateTrunc(UserInst->getOperand(0),
                                              UserInstType, "_lvi_trunc_e");
        UserInst->replaceAllUsesWith(Trunc);
        InstructionsToErase.insert(UserInst);
      } else if (UserInst->getOpcode() == Instruction::InsertElement) {
        auto* Vector = dyn_cast<llvm::VectorType>(UserInst->getOperand(0)->getType());
        auto* VectorElementType = dyn_cast<IntegerType>(Vector->getElementType());

        if (!VectorElementType) {
          continue;
        }

        for (size_t i = 1; i < UserInst->getNumOperands(); i++) {
          Value* Operand = UserInst->getOperand(i);
          auto* OperandType = dyn_cast<IntegerType>(Operand->getType());

          if (!OperandType || Operand->getType() == VectorElementType) {
            continue;
          }

          Builder.SetInsertPoint(UserInst);
          if (OperandType->getBitWidth() > VectorElementType->getBitWidth()) {
            llvm::Value* Trunc =
                Builder.CreateTrunc(UserInst->getOperand(i), VectorElementType, "_lvi_trunc_c");
            UserInst->setOperand(i, Trunc);
          } else {
            llvm::Value* Ext = Builder.CreateSExt(UserInst->getOperand(i),
                                                  VectorElementType, "_lvi_sext_e");
            UserInst->setOperand(i, Ext);
          }
        }
      } else if (UserInst->getOpcode() == Instruction::InsertValue) {
        uint32_t InsertIndex = dyn_cast<llvm::InsertValueInst>(UserInst)->getIndices()[0];
        Type* AggregateType = UserInst->getType();
        auto* AggregateElementType = dyn_cast<IntegerType>(AggregateType->getContainedType(InsertIndex));

        if (!AggregateElementType) {
          continue;
        }

        debug << "InsertValue into index " << InsertIndex << " of " << *AggregateType
              << ", which is an " << *AggregateElementType << "\n";

        Value* Operand = UserInst->getOperand(1);
        auto* OperandType = dyn_cast<IntegerType>(Operand->getType());
        if (!OperandType || OperandType == AggregateElementType) {
          continue;
        }

        Builder.SetInsertPoint(UserInst);
        if (OperandType->getBitWidth() > AggregateElementType->getBitWidth()) {
          llvm::Value* Trunc =
              Builder.CreateTrunc(Operand, AggregateElementType, "_lvi_truncf");
          UserInst->setOperand(1, Trunc);
        } else {
          llvm::Value* Ext = Builder.CreateSExt(Operand,
                                                AggregateElementType, "_lvi_sext_f");
          UserInst->setOperand(1, Ext);
        }
        // }
      } else if (UserInst->getOpcode() == Instruction::Switch) {
        auto* Switch = dyn_cast<llvm::SwitchInst>(UserInst);
        assert(Switch);

        // Condition should be a value we rewrote
        auto* ConditionType = dyn_cast<IntegerType>(Switch->getCondition()->getType());
        assert(ConditionType);

        for (const llvm::SwitchInst::CaseHandle& Case : Switch->cases()) {
          auto* NewConstant = dyn_cast<llvm::ConstantInt>(llvm::ConstantInt::get(
              ConditionType, Case.getCaseValue()->getUniqueInteger().trunc(ConditionType->getBitWidth())));
          Case.setValue(NewConstant);
        }
      } else if (UserInst->getOpcode() == Instruction::BitCast) {
        // InType is a type we modified, out type is not
        Value* BitCastIn = UserInst->getOperand(0);
        auto* InType = dyn_cast<llvm::IntegerType>(BitCastIn->getType());
        Type* OutType = UserInst->getType();
        size_t OutTypeBitWidth = OutType->getPrimitiveSizeInBits();
        IntegerType* OutTypeEquivalentInteger = Type::getIntNTy(Ctx, OutTypeBitWidth);

        debug << "Considering bitcast " << *UserInst << "\n";
        debug << "Casting " << *InType << " -> " << *OutType << " (width " << OutTypeBitWidth << ")\n";
        debug << "Equivalent integer type is" << OutTypeEquivalentInteger << ")\n";

        assert(InType);

        Builder.SetInsertPoint(UserInst);
        if (InType->getBitWidth() > OutTypeBitWidth) {
          llvm::Value* Trunc =
              Builder.CreateTrunc(BitCastIn, OutTypeEquivalentInteger, "_lvi_trunc_c");
          UserInst->setOperand(0, Trunc);
        } else {
          llvm::Value* Ext = Builder.CreateSExt(BitCastIn,
                                                OutTypeEquivalentInteger, "_lvi_sext_e");
          UserInst->setOperand(0, Ext);
        }
      }

      for (auto& [Use, NewValue] : UsesToReplace) {
        debug << "Setting " << **Use << " to " << *NewValue << "\n";
        Use->set(NewValue);
      }
    }

    debug << "====:(){ :|:& };:&==== " << F.getName() << " FINAL IR ";
    debug << "====:(){:|:&}:====\n";
    debug << F << "\n\n";

    // for (Instruction* I : InstructionsToErase) {
    //   I->eraseFromParent();
    // }
    // InstructionsToErase.clear();

    for (auto& BB : F) {
      for (auto& I : BB) {
        if (I.getOpcode() == Instruction::SExt ||
            I.getOpcode() == Instruction::Trunc) {
          if (I.getOperand(0)->getType() == I.getType()) {
            debug << "I'm fucking with a sext or a trunc: " << I << "\n";
            I.replaceAllUsesWith(I.getOperand(0));
            InstructionsToErase.insert(&I);
          }
        }
      }
    }

    for (Instruction* I : InstructionsToErase) {
      debug << "Erasing instruction at " << I << ":" << *I << "\n";
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
            ToPack->to_pack.emplace_back(&I, 64 - ToPack->bits_remaining,
                                         IntType);
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
          llvm::Value* Unpacked =
              doUnpack(Packed, Index, IntType, Builder, Ctx);
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
                // FPM.addPass(llvm::VerifierPass());
                return true;
              }
              if (Name == "wdsymys-packing") {
                FPM.addPass(WDSYMYSPackingPass());
                // FPM.addPass(llvm::VerifierPass());
                return true;
              }
              return false;
            });
        PB.registerOptimizerLastEPCallback([&](llvm::ModulePassManager& MPM,
                                               auto) {  
          MPM.addPass(createModuleToFunctionPassAdaptor(llvm::PromotePass()));
          MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSLVIPass()));
          MPM.addPass(createModuleToFunctionPassAdaptor(WDSYMYSPackingPass()));
          MPM.addPass(createModuleToFunctionPassAdaptor(llvm::AggressiveInstCombinePass()));
          MPM.addPass(createModuleToFunctionPassAdaptor(llvm::VerifierPass()));
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
