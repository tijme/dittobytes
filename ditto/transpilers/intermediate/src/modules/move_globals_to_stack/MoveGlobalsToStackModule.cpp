/**
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###################                                         ###################
 * ################### THIS IS AN EXTERNAL LIBRARY FROM GITHUB ###################
 * ###################                                         ###################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * 
 * The author and owner of this specific function pass is the SheLLVM organisation: 
 * https://github.com/SheLLVM/SheLLVM. Copyright may apply. Please refer to the 
 * original source.
 * 
 * Original source: https://github.com/SheLLVM/SheLLVM/tree/master
 */

/**
 * Copyright (c) 2018 SheLLVM Development Team. All rights reserved.
 * 
 * Developed by: SheLLVM Development Team
 * https://github.com/SheLLVM
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal with the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimers.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimers in the documentation and/or other materials provided with the distribution.
 * Neither the names of SheLLVM Development Team, SheLLVM, nor the names of its contributors may be used to endorse or promote products derived from this Software without specific prior written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
 */

/**
 * LLVM includes
 */
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/GlobalOpt.h"

/**
 * Namespace(s) to use
 */
using namespace std;
using namespace llvm;

/**
 * A class to move globals to the stack.
 */
class MoveGlobalsToStackModule {

private:

    /**
     * Whether the module is enabled (default) or disabled.
     * 
     * @returns bool Positive if enabled.
     */
    bool moduleIsEnabled() {
        const char* MOVE_GLOBALS_TO_STACK = std::getenv("MOVE_GLOBALS_TO_STACK");
        return (MOVE_GLOBALS_TO_STACK && std::string(MOVE_GLOBALS_TO_STACK) == "true");
    }

public:

    /**
     * Main execution method for the MoveGlobalsToStackModule class.
     *
     * @param Module& M The intermediate module to run on.
     * @param ModuleAnalysisManager& The LLVM module analysis manager.
     * @return bool Indicates if the intermediate module was modified.
     */
    bool run(Module &M, ModuleAnalysisManager &) {
        // Ensure module is enabled
        if (!moduleIsEnabled()) return false;

        // Inform user that we are running this module
        dbgs() << "        ↳ Running MoveGlobalsToStackModule module.\n";

        // Build: global → all functions using it
        SmallMapVector<GlobalVariable *, SmallSetVector<Function *, 4>, 4> globalsToFunctions;

        for (GlobalVariable &G : M.globals()) {
            if (!G.isDiscardableIfUnused()) continue;
            SmallSetVector<Function *, 4> funcs;
            if (collectUsingFunctions(G, funcs) && !funcs.empty())
                globalsToFunctions[&G] = std::move(funcs);
        }

        if (globalsToFunctions.empty()) return false;

        // Build reverse map: function → globals to inline into it
        SmallMapVector<Function *, SmallSetVector<GlobalVariable *, 4>, 4> usage;
        for (auto &KV : globalsToFunctions)
            for (Function *F : KV.second)
                usage[F].insert(KV.first);

        // Create a per-function stack copy for each global
        for (auto &KV : usage)
            inlineGlobals(M, KV.first, KV.second);

        // Erase globals that are now fully replaced
        for (auto &KV : globalsToFunctions)
            if (KV.first->use_empty())
                KV.first->eraseFromParent();

        return true;
    }

private:

    // Collect every Function that (directly or transitively through ConstantExprs)
    // uses V. Returns false if a non-inlinable user (non-discardable GlobalVariable
    // or non-Instruction) is found, meaning the global cannot be moved to the stack.
    bool collectUsingFunctions(Value &V, SmallSetVector<Function *, 4> &Functions) {
        SmallVector<User *, 4> Worklist;
        SmallSet<User *, 4> Visited;

        for (auto *U : V.users())
            Worklist.push_back(U);

        while (!Worklist.empty()) {
            auto *U = Worklist.pop_back_val();

            if (Visited.count(U)) continue;
            Visited.insert(U);

            if (isa<ConstantExpr>(U) || isa<ConstantAggregate>(U) || isa<GlobalVariable>(U)) {
                if (isa<GlobalVariable>(U) && !cast<GlobalVariable>(U)->isDiscardableIfUnused())
                    return false;
                for (auto *UU : U->users())
                    Worklist.push_back(UU);
                continue;
            }

            auto *I = dyn_cast<Instruction>(U);
            if (!I) return false;
            Functions.insert(I->getParent()->getParent());
        }

        return true;
    }

    void inlineGlobals(Module &M, Function *F, SmallSetVector<GlobalVariable *, 4> &Vars) {
        BasicBlock &BB = F->getEntryBlock();
        Instruction *insertionPoint = &*BB.getFirstInsertionPt();
        LLVMContext &Ctx = F->getContext();

        IRBuilder<> builder(insertionPoint);

        SmallMapVector<GlobalVariable *, Value *, 4> Replacements;
        StoreInst * firstStore = nullptr;

        for (auto *G : Vars) {
            Constant *initializer = G->getInitializer();
            Type *globalType = G->getValueType();

            dbgs() << "        ↳ Found a global variable to inline.\n";

            if (!initializer) {
                dbgs() << "        ↳ Skipping global (no initializer): " << G->getName() << "\n";
                continue;
            }

            bool isString = false;

            if (auto *CA = dyn_cast<ConstantDataArray>(initializer)) {
                if (CA->isString()) {

                    isString = true;

                    // Allocate one contiguous stack buffer for the entire global
                    AllocaInst *alloca = builder.CreateAlloca(globalType, nullptr, G->getName() + ".stack");
                    Replacements[G] = alloca;

                    // Get size in bytes of the global type
                    uint64_t sizeInBytes = M.getDataLayout().getTypeAllocSize(globalType);

                    // Flatten initializer to raw bytes
                    SmallVector<uint8_t, 64> data;
                    {
                        // Use DataLayout helper to get raw bytes from constant initializer
                        // Since LLVM has no direct API, use ConstantDataSequential or ConstantAggregate parsing:
                        if (auto *CDS = dyn_cast<ConstantDataSequential>(initializer)) {
                            StringRef raw = CDS->getRawDataValues();
                            data.append(raw.bytes_begin(), raw.bytes_end());
                        } else if (auto *CI = dyn_cast<ConstantInt>(initializer)) {
                            uint64_t val = CI->getZExtValue();
                            data.resize(sizeInBytes, 0);
                            memcpy(data.data(), &val, std::min(sizeInBytes, static_cast<uint64_t>(8)));
                        } else if (auto *CA = dyn_cast<ConstantAggregate>(initializer)) {
                            // Rough fallback: serialize each element recursively (not implemented here)
                            // Just zero fill to be safe
                            data.resize(sizeInBytes, 0);
                        } else {
                            // Unsupported initializer type, zero initialize
                            data.resize(sizeInBytes, 0);
                        }
                    }

                    // Store bytes in largest chunks into alloca sequentially
                    uint64_t offset = 0;
                    while (offset < sizeInBytes) {
                        uint64_t remaining = sizeInBytes - offset;
                        Type *writeType;
                        size_t writeSize;

                        if (remaining >= 8) {
                            writeType = Type::getInt64Ty(Ctx);
                            writeSize = 8;
                        } else if (remaining >= 4) {
                            writeType = Type::getInt32Ty(Ctx);
                            writeSize = 4;
                        } else if (remaining >= 2) {
                            writeType = Type::getInt16Ty(Ctx);
                            writeSize = 2;
                        } else {
                            writeType = Type::getInt8Ty(Ctx);
                            writeSize = 1;
                        }

                        uint64_t chunk = 0;
                        memcpy(&chunk, data.data() + offset, writeSize);

                        // Compute pointer to offset bytes inside alloca
                        // Value *bytePtr = builder.CreateBitCast(alloca, Type::getInt8PtrTy(Ctx));
                        Value *bytePtr = builder.CreateBitCast(alloca, builder.getPtrTy());

                        Value *gepPtr = builder.CreateGEP(Type::getInt8Ty(Ctx), bytePtr,
                                                         ConstantInt::get(Type::getInt64Ty(Ctx), offset));
                        Value *castedPtr = builder.CreateBitCast(gepPtr, PointerType::getUnqual(writeType));

                        builder.CreateStore(ConstantInt::get(writeType, chunk), castedPtr);

                        offset += writeSize;
                    }

                }
            }

            if (!isString) {

                Instruction * inst =
                    new AllocaInst(G -> getValueType(), G -> getType() -> getAddressSpace(),
                        nullptr, G -> getAlign().valueOrOne(), "",
                        firstStore ? firstStore : insertionPoint);

                inst -> takeName(G);

                Replacements[G] = inst;

                if (G -> hasInitializer()) {
                    Constant * initializer = G -> getInitializer();
                    StoreInst * store = new StoreInst(initializer, inst, insertionPoint);

                    extractValuesFromStore(store, Vars);

                    if (!firstStore)
                        firstStore = store;
                }



            }
            
        }

        // Replace uses of each global within F only (other functions get their own copy)
        for (auto *G : Vars) {
            if (!Replacements.count(G))
                continue;

            Value *replacement = Replacements[G];

            SmallVector<User *, 8> users(G->users());
            for (User *U : users) {
                if (auto *CE = dyn_cast<ConstantExpr>(U)) {
                    SmallVector<User *, 8> CEUsers(CE->users());
                    for (User *CEUser : CEUsers) {
                        if (Instruction *I = dyn_cast<Instruction>(CEUser)) {
                            if (I->getParent()->getParent() != F) continue;
                            IRBuilder<> ib(I);
                            Value *replacementVal = CE;
                            if (CE->getOpcode() == Instruction::BitCast) {
                                replacementVal = ib.CreateBitCast(replacement, CE->getType());
                            }
                            I->replaceUsesOfWith(CE, replacementVal);
                        }
                    }
                } else if (Instruction *I = dyn_cast<Instruction>(U)) {
                    if (I->getParent()->getParent() != F) continue;
                    I->replaceUsesOfWith(G, replacement);
                }
            }
            // Do not erase G here — run() erases it once all functions are done
        }
    }




    // Copied from GlobalOpt.cpp
    void makeAllConstantUsesInstructions(Constant * C) {
        SmallVector < ConstantExpr * , 4 > Users;
        for (auto * U: C -> users()) {
            if (auto * CE = dyn_cast < ConstantExpr > (U))
                Users.push_back(CE);
            else
                // We should never get here; allNonInstructionUsersCanBeMadeInstructions
                // should not have returned true for C.
                assert(
                    isa < Instruction > (U) &&
                    "Can't transform non-constantexpr non-instruction to instruction!");
        }

        SmallVector < Instruction * , 4 > CEUsers;
        for (auto * U: Users) {
            // DFS DAG traversal of U to eliminate ConstantExprs recursively
            ConstantExpr * CE = nullptr;

            do {
                CE = U; // Start by trying to destroy the root

                CEUsers.clear();
                auto it = CE -> user_begin();
                while (it != CE -> user_end()) {
                    if (isa < ConstantExpr > ( * it)) {
                        // Recursive ConstantExpr found; switch to it
                        CEUsers.clear();
                        CE = cast < ConstantExpr > ( * it);
                        it = CE -> user_begin();
                    } else {
                        // Function; add to UUsers
                        CEUsers.push_back(cast < Instruction > ( * it));
                        it++;
                    }
                }

                // All users of CE are instructions; replace CE with an instruction for
                // each
                for (auto * CEU: CEUsers) {
                    Instruction * NewU = CE -> getAsInstruction();
                    NewU -> insertBefore(CEU);
                    CEU -> replaceUsesOfWith(CE, NewU);
                }

                // We've replaced all the uses, so destroy the constant. (destroyConstant
                // will update value handles and metadata.)
                CE -> destroyConstant();
            } while (CE != U); // Continue until U is destroyed
        }
    }



    void disaggregateVars(
        Instruction * After, Value * Ptr, SmallVectorImpl < Value * > & Idx,
        ConstantAggregate & C, SmallSetVector < GlobalVariable * , 4 > & Vars) {
        SmallSetVector < Value * , 4 > ToUndefine;

        Constant * C2;
        for (unsigned i = 0;
            (C2 = C.getAggregateElement(i)); i++) {
            Idx.push_back(ConstantInt::get(
                Type::getInt32Ty(After -> getParent() -> getContext()), i));

            if (isa < ConstantAggregate > (C2)) {
                disaggregateVars(After, Ptr, Idx, cast < ConstantAggregate > ( * C2), Vars);

            } else if (isa < ConstantExpr > (C2) ||
                (isa < GlobalVariable > (C2) &&
                    Vars.count(cast < GlobalVariable > (C2)))) {
                GetElementPtrInst * GEP =
                    GetElementPtrInst::CreateInBounds(C.getType(), Ptr, Idx);
                GEP -> insertAfter(After);

                ToUndefine.insert(C2);

                new StoreInst(C2, GEP, GEP -> getNextNode());
            }

            Idx.pop_back();
        }

        for (auto * V: ToUndefine)
            C.handleOperandChange(V, UndefValue::get(V -> getType()));
    }

    void extractValuesFromStore(
        StoreInst * inst, SmallSetVector < GlobalVariable * , 4 > & Vars) {
        Value * V = inst -> getValueOperand();
        if (!isa < ConstantAggregate > (V))
            return;

        SmallVector < Value * , 4 > Idx;
        Idx.push_back(
            ConstantInt::get(Type::getInt32Ty(inst -> getParent() -> getContext()), 0));

        disaggregateVars(inst, inst -> getPointerOperand(), Idx,
            cast < ConstantAggregate > ( * V), Vars);
    }

};