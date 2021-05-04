/*
 * Copyright 2021 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Analyzes stores and loads of non-local state, and optimizes them in various
// ways. For example, a store that is never read can be removed as dead.
//
// "Store" is used generically here to mean a write to a non-local location,
// which includes:
//
//  * Stores to linear memory (Store).
//  * Stores to globals (GlobalSet).
//  * Stores to GC data (StructSet, ArraySet)
//
// This pass optimizes all of the above. It does so using a generic framework in
// order to share as much code as possible between them. This has downsides for
// globals, in particular, as they could be optimized with an IR that is tailor-
// made for scanning of global indexes (much as we do in our analyses of locals
// in other places). However, global operations are also less common than memory
// and GC operations, so hopefully the tradeoff is reasonable.
//

#include <cfg/cfg-traversal.h>
#include <ir/effects.h>
#include <ir/local-graph.h>
#include <ir/properties.h>
#include <ir/replacer.h>
#include <pass.h>
#include <support/unique_deferring_queue.h>
#include <wasm-builder.h>
#include <wasm.h>

namespace wasm {

namespace {

// Information in a basic block.
struct BasicBlockInfo {
  // The list of relevant expressions, that are either stores or things that
  // interact with stores.
  std::vector<Expression*> exprs;
};

// A variation of LocalGraph that can also compare expressions to check for
// their equivalence. Basic LocalGraph just looks at locals, while this class
// goes further and looks at the structure of the expression, taking into
// account fallthrough values and other factors, in order to handle common
// cases of obviously-equivalent things.
struct ComparingLocalGraph : public LocalGraph {
  PassOptions& passOptions;
  FeatureSet features;

  ComparingLocalGraph(Function* func,
                      PassOptions& passOptions,
                      FeatureSet features)
    : LocalGraph(func), passOptions(passOptions), features(features) {}

  // Check whether the values of two expressions are definitely identical. This
  // is important for stores and loads that receive an input (like GC data),
  // since we need to see that the pointer input is equivalent before we can
  // tell if two stores overlap.
  // TODO: move to LocalGraph if we find more users?
  bool equivalent(Expression* a, Expression* b) {
    a = Properties::getFallthrough(a, passOptions, features);
    b = Properties::getFallthrough(b, passOptions, features);
    if (auto* aGet = a->dynCast<LocalGet>()) {
      if (auto* bGet = b->dynCast<LocalGet>()) {
        if (LocalGraph::equivalent(aGet, bGet)) {
          return true;
        }
      }
    }
    if (auto* aConst = a->dynCast<Const>()) {
      if (auto* bConst = b->dynCast<Const>()) {
        return aConst->value == bConst->value;
      }
    }
    return false;
  }
};

// Parent class of all implementations of the logic of identifying stores etc.
struct Logic {
  Function* func;

  Logic(Function* func, PassOptions& passOptions, FeatureSet features)
    : func(func) {}

  // Hooks for subclasses to override.

  // Returns whether an expression is a relevant store for us to consider.
  bool isStore(Expression* curr) { WASM_UNREACHABLE("unimp"); };

  // Returns whether an expression is a relevant load for us to consider.
  bool isLoad(Expression* curr) { WASM_UNREACHABLE("unimp"); };

  // Returns whether an expression may interact with loads and stores in
  // interesting ways
  bool mayInteract(Expression* curr,
                      const ShallowEffectAnalyzer& currEffects) {
    WASM_UNREACHABLE("unimp");
  }

  // Returns whether the expression is a barrier to our analysis: something that
  // we should stop when we see it, because it could do things that we cannot
  // analyze. A barrier will definitely pose a problem for us, as opposed to
  // something mayInteract() returns true for - we will check for interactions
  // later for mayInteract()s, but with barriers we don't need to.
  //
  // The default behavior here considers all calls to be barriers. Subclasses
  // can use whole-program information to do better.
  bool isBarrier(Expression* curr, const ShallowEffectAnalyzer& currEffects) {
    // TODO: ignore throws of an exception that is definitely caught in this
    //       function
    // TODO: if we add an "ignore after trap mode" (to assume nothing happens
    //       after a trap) then we could stop assuming any trap can lead to
    //       access of global data.
    return currEffects.calls || currEffects.throws || currEffects.trap ||
           currEffects.branchesOut;
  };

  // Returns whether an expression is a load that corresponds to a store, that
  // is, that loads the exact data that the store writes.
  bool isLoadFrom(Expression* curr,
                  const ShallowEffectAnalyzer& currEffects,
                  Expression* store) {
    WASM_UNREACHABLE("unimp");
  };

  // Returns whether an expression isTrample a store completely, overwriting all
  // the store's written data.
  // This is only called if isLoadFrom() returns false, as we assume there is no
  // single instruction that can do both.
  bool isTrample(Expression* curr,
                 const ShallowEffectAnalyzer& currEffects,
                 Expression* store) {
    WASM_UNREACHABLE("unimp");
  };

  // Returns whether an expression may interact with another in a way that we
  // cannot fully analyze, and so we must give up and assume the very worst.
  // This is only called if isLoadFrom() and isTrample() both return false.
  //
  // This is similar to mayInteract(), but considers a specific interaction with
  // another particular expression.
  bool mayInteractWith(Expression* curr,
                       const ShallowEffectAnalyzer& currEffects,
                        Expression* store) {
    WASM_UNREACHABLE("unimp");
  };

  // Given a store that is not needed, get drops of its children to replace it
  // with. This effectively removes the store without removes its children.
  Expression* replaceStoreWithDrops(Expression* store, Builder& builder) {
    WASM_UNREACHABLE("unimp");
  };
};

// Represent all barriers in a simple way.
static Expression* const barrier = nullptr;

// Core code to generate the relevant CFG, analyze it, and optimize it.
//
// This is as generic as possible over what a "store" actually is; all the
// specific logic of handling globals vs memory vs the GC heap is all left to a
// to a LogicType that this is templated on, which subclasses from Logic.
template<typename LogicType>
struct DeadStoreCFG
  : public CFGWalker<DeadStoreCFG<LogicType>,
                     UnifiedExpressionVisitor<DeadStoreCFG<LogicType>>,
                     BasicBlockInfo> {
  Function* func;
  PassOptions& passOptions;
  FeatureSet features;
  LogicType logic;

  DeadStoreCFG(Module* wasm, Function* func, PassOptions& passOptions)
    : func(func), passOptions(passOptions), features(wasm->features),
      logic(func, passOptions, wasm->features) {
    this->setModule(wasm);
  }

  ~DeadStoreCFG() {}

  void visitExpression(Expression* curr) {
    if (!this->currBasicBlock) {
      return;
    }

    ShallowEffectAnalyzer currEffects(passOptions, features, curr);

    auto& exprs = this->currBasicBlock->contents.exprs;

    // Add all relevant things to the list of exprs for the current basic block.
    if (logic.isStore(curr) || logic.isLoad(curr) ||
        logic.mayInteract(curr, currEffects)) {
      exprs.push_back(curr);
    } else if (logic.isBarrier(curr, currEffects)) {
      // Barriers can be very common, so as a minor optimization avoid having
      // consecutive ones; a single barrier will stop us.
      if (exprs.empty() || exprs.back() != barrier) {
        exprs.push_back(barrier);
      }
    }
  }

  // All the stores we can optimize, that is, stores that write to a non-local
  // place from which we have a full understanding of all the loads. This data
  // structure maps such an understood store to the list of loads for it. In
  // particular, if that list is empty then the store is dead (since we have
  // a full understanding of all the loads, and there are none), and if the list
  // is non-empty then only those loads read that store's value, and nothing
  // else.
  std::unordered_map<Expression*, std::vector<Expression*>> understoodStores;

  using Self = DeadStoreCFG<LogicType>;

  using BasicBlock = typename CFGWalker<Self,
                                        UnifiedExpressionVisitor<Self>,
                                        BasicBlockInfo>::BasicBlock;

  void analyze() {
    // create the CFG by walking the IR
    this->walkFunction(func);

    // Flow the values and conduct the analysis. This finds each relevant store
    // and then flows from it to all possible uses through the CFG.
    //
    // TODO: Optimize. This is a pretty naive way to flow the values, but it
    //       should be reasonable assuming most stores are quickly seen as
    //       having possible interactions (e.g., the first time we see a call)
    //       and so most flows are halted very quickly. A much faster lane-based
    //       flow is possible for some things like globals, but that would not
    //       work for things like memory and GC which do not have simple "lanes"
    //       as they have a pointer input as well and not just an absolute
    //       location that we can interprete as a "lane".

    for (auto& block : this->basicBlocks) {
      for (size_t i = 0; i < block->contents.exprs.size(); i++) {
        auto* store = block->contents.exprs[i];

        if (store == barrier || !logic.isStore(store)) {
          continue;
        }

        // The store is assumed to be understood (and hence present on the map)
        // until we see a problem.
        understoodStores[store];

        // Flow this store forward through basic blocks, looking for what it
        // affects and interacts with.
        UniqueNonrepeatingDeferredQueue<BasicBlock*> work;

        // When we find something we cannot optimize, stop flowing and mark the
        // store as unoptimizable.
        auto halt = [&]() {
          work.clear();
          understoodStores.erase(store);
        };

        // Scan through a block, starting from a certain position, looking for
        // loads, isTrample, and other interactions with our store.
        auto scanBlock = [&](BasicBlock* block, size_t from) {
          for (size_t i = from; i < block->contents.exprs.size(); i++) {
            auto* curr = block->contents.exprs[i];

            if (curr == barrier) {
              halt();
              return;
            }

            ShallowEffectAnalyzer currEffects(passOptions, features, curr);

            if (logic.isLoadFrom(curr, currEffects, store)) {
              // We found a definite load of this store, note it.
              understoodStores[store].push_back(curr);
            } else if (logic.isTrample(curr, currEffects, store)) {
              // We do not need to look any further along this block, or in
              // anything it can reach, as this store has been trampled.
              return;
            } else if (logic.mayInteractWith(curr, currEffects, store)) {
              // Stop: we cannot fully analyze the uses of this store as there
              // are interactions we cannot see.
              halt();
              return;
            }
          }

          // We reached the end of the block, prepare to flow onward.
          for (auto* out : block->out) {
            work.push(out);
          }

          if (block == this->exit) {
            // Any value flowing out can be reached by global code outside the
            // function after we leave.
            halt();
          }
        };

        // First, start in the current location in the block, right after the
        // store itself.
        scanBlock(block.get(), i + 1);

        // Next, continue flowing through other blocks.
        while (!work.empty()) {
          auto* curr = work.pop();
          scanBlock(curr, 0);
        }
      }
    }
  }

  // Optimizes the function, and returns whether we made any changes.
  bool optimize() {
    analyze();

    Builder builder(*this->getModule());

    ExpressionReplacer replacer;

    // Optimize the stores that have no unknown interactions.
    for (auto& kv : understoodStores) {
      auto* store = kv.first;
      const auto& loads = kv.second;
      if (loads.empty()) {
        // This store has no loads, which means it is trampled by other stores
        // before it is read, and so it can just be dropped.
        //
        // Note that this is valid even if we care about implicit traps, such as
        // a trap from a store that is out of bounds. We are removing one store,
        // but it was trampled later, which means that a trap will still occur
        // at that time; furthermore, we do not delay the trap in a noticeable
        // way since if the path between the stores crosses anything that
        // affects global state then we would not have considered the store to
        // be trampled (it could have been read there).
        replacer.replacements[store] =
          logic.replaceStoreWithDrops(store, builder);
      }
      // TODO: When there are loads, we can replace the loads as well (by saving
      //       the value to a local for that global, etc.).
      //       Note that we may need to leave the loads if they have side
      //       effects, like a possible trap on memory loads, but they can be
      //       left as dropped, the same as with store inputs.
    }

    if (replacer.replacements.empty()) {
      return false;
    }

    replacer.walk(this->func->body);

    return true;
  }
};

// A logic that uses a local graph, as it needs to compare pointers.
struct ComparingLogic : public Logic {
  ComparingLocalGraph localGraph;

  ComparingLogic(Function* func, PassOptions& passOptions, FeatureSet features)
    : Logic(func, passOptions, features),
      localGraph(func, passOptions, features) {}
};

// Optimize module globals: GlobalSet/GlobalGet.
struct GlobalLogic : public Logic {
  GlobalLogic(Function* func, PassOptions& passOptions, FeatureSet features)
    : Logic(func, passOptions, features) {}

  bool isStore(Expression* curr) { return curr->is<GlobalSet>(); }

  bool isLoad(Expression* curr) { return curr->is<GlobalGet>(); }

  bool mayInteract(Expression* curr,
                      const ShallowEffectAnalyzer& currEffects) {
    return false;
  }

  bool isLoadFrom(Expression* curr,
                  const ShallowEffectAnalyzer& currEffects,
                  Expression* store_) {
    if (auto* load = curr->dynCast<GlobalGet>()) {
      auto* store = store_->cast<GlobalSet>();
      return load->name == store->name;
    }
    return false;
  }

  bool isTrample(Expression* curr,
                 const ShallowEffectAnalyzer& currEffects,
                 Expression* store_) {
    if (auto* otherStore = curr->dynCast<GlobalSet>()) {
      auto* store = store_->cast<GlobalSet>();
      return otherStore->name == store->name;
    }
    return false;
  }

  bool mayInteractWith(Expression* curr,
                   const ShallowEffectAnalyzer& currEffects,
                   Expression* store) {
    // We have already handled everything in isLoadFrom() and isTrample().
    return false;
  }

  Expression* replaceStoreWithDrops(Expression* store, Builder& builder) {
    return builder.makeDrop(store->cast<GlobalSet>()->value);
  }
};

// Optimize memory stores/loads.
struct MemoryLogic : public ComparingLogic {
  MemoryLogic(Function* func, PassOptions& passOptions, FeatureSet features)
    : ComparingLogic(func, passOptions, features) {}

  bool isStore(Expression* curr) { return curr->is<Store>(); }

  bool isLoad(Expression* curr) { return curr->is<Load>(); }

  bool mayInteract(Expression* curr,
                      const ShallowEffectAnalyzer& currEffects) {
    return currEffects.readsMemory || currEffects.writesMemory;
  }

  bool isLoadFrom(Expression* curr,
                  const ShallowEffectAnalyzer& currEffects,
                  Expression* store_) {
    if (curr->type == Type::unreachable) {
      return false;
    }
    if (auto* load = curr->dynCast<Load>()) {
      auto* store = store_->cast<Store>();
      // Atomic stores are dangerous, since they have additional trapping
      // behavior - they trap on unaligned addresses. For simplicity, only
      // consider the case where atomicity is identical.
      // TODO: support ignoreImplicitTraps
      if (store->isAtomic != load->isAtomic) {
        return false;
      }
      // TODO: For now, only handle the obvious case where the
      // operations are identical in size and offset.
      return load->bytes == store->bytes &&
             load->bytes == load->type.getByteSize() &&
             load->offset == store->offset &&
             localGraph.equivalent(load->ptr, store->ptr);
    }
    return false;
  }

  bool isTrample(Expression* curr,
                 const ShallowEffectAnalyzer& currEffects,
                 Expression* store_) {
    if (auto* otherStore = curr->dynCast<Store>()) {
      auto* store = store_->cast<Store>();
      // As in isLoadFrom, atomic stores are dangerous.
      if (store->isAtomic != otherStore->isAtomic) {
        return false;
      }
      // TODO: compare in detail. For now, handle the obvious case where the
      // stores are identical in size, offset, etc., so that identical repeat
      // stores are handled.
      return otherStore->bytes == store->bytes &&
             otherStore->offset == store->offset &&
             localGraph.equivalent(otherStore->ptr, store->ptr);
    }
    return false;
  }

  bool mayInteractWith(Expression* curr,
                   const ShallowEffectAnalyzer& currEffects,
                   Expression* store) {
    // Anything we did not identify so far is dangerous.
    return currEffects.readsMemory || currEffects.writesMemory;
  }

  Expression* replaceStoreWithDrops(Expression* store, Builder& builder) {
    auto* castStore = store->cast<Store>();
    return builder.makeSequence(builder.makeDrop(castStore->ptr),
                                builder.makeDrop(castStore->value));
  }
};

// Optimize GC data: StructGet/StructSet.
// TODO: Arrays.
struct GCLogic : public ComparingLogic {
  GCLogic(Function* func, PassOptions& passOptions, FeatureSet features)
    : ComparingLogic(func, passOptions, features) {}

  bool isStore(Expression* curr) { return curr->is<StructSet>(); }

  bool isLoad(Expression* curr) { return curr->is<StructGet>(); }

  bool mayInteract(Expression* curr,
                      const ShallowEffectAnalyzer& currEffects) {
    return currEffects.readsHeap || currEffects.writesHeap;
  }

  bool isLoadFrom(Expression* curr,
                  const ShallowEffectAnalyzer& currEffects,
                  Expression* store_) {
    if (auto* load = curr->dynCast<StructGet>()) {
      auto* store = store_->cast<StructSet>();
      // Note that we do not need to check the type: we check that the
      // reference is equivalent, and if it is then the types must be compatible
      // in addition to them pointing to the same memory.
      return localGraph.equivalent(load->ref, store->ref) &&
             load->index == store->index;
    }
    return false;
  }

  bool isTrample(Expression* curr,
                 const ShallowEffectAnalyzer& currEffects,
                 Expression* store_) {
    if (auto* otherStore = curr->dynCast<StructSet>()) {
      auto* store = store_->cast<StructSet>();
      // See note in isLoadFrom about typing.
      return localGraph.equivalent(otherStore->ref, store->ref) &&
             otherStore->index == store->index;
    }
    return false;
  }

  // Check whether two GC operations may alias memory.
  template<typename U, typename V> bool mayAlias(U* u, V* v) {
    // If the index does not match, no aliasing is possible.
    if (u->index != v->index) {
      return false;
    }
    // Even if the index is identical, aliasing still may be impossible if the
    // types are not compatible. To check that, find the least upper bound
    // (which always exists - the empty struct if nothing else), and then see if
    // the index is included in that upper bound. If it is, then the types are
    // compatible enough to alias memory.
    auto lub = Type::getLeastUpperBound(u->ref->type, v->ref->type);
    if (u->index >= lub.getHeapType().getStruct().fields.size()) {
      return false;
    }
    // We don't know, so assume they can alias.
    return true;
  }

  bool mayInteractWith(Expression* curr,
                   const ShallowEffectAnalyzer& currEffects,
                   Expression* store_) {
    auto* store = store_->cast<StructSet>();
    // We already checked isLoadFrom and isTrample and it was neither of those,
    // so just check if the memory can possibly alias.
    if (auto* otherStore = curr->dynCast<StructSet>()) {
      return mayAlias(otherStore, store);
    }
    if (auto* load = curr->dynCast<StructGet>()) {
      return mayAlias(load, store);
    }
    // This is not a load or a store that we recognize; check for generic heap
    // interactions.
    return currEffects.readsHeap || currEffects.writesHeap;
  }

  Expression* replaceStoreWithDrops(Expression* store, Builder& builder) {
    auto* castStore = store->cast<StructSet>();
    return builder.makeSequence(builder.makeDrop(castStore->ref),
                                builder.makeDrop(castStore->value));
  }
};

// Perform dead store elimination 100% locally, that is, without any whole-
// program analysis. This is not very powerful, but can catch simple patterns of
// obviously dead stores, and is useful for testing.
//
// This does all the optimizations (globals, memory, GC) in sequence, which is
// good for cache locality. That is the reason there are not separate passes for
// each one. TODO: the optimizations can share more things between them
struct LocalDeadStoreElimination
  : public WalkerPass<PostWalker<LocalDeadStoreElimination>> {
  bool isFunctionParallel() { return true; }

  Pass* create() { return new LocalDeadStoreElimination; }

  void doWalkFunction(Function* func) {
    // Optimize globals.
    DeadStoreCFG<GlobalLogic>(getModule(), func, getPassOptions()).optimize();

    // Optimize memory.
    // TODO: should this run when not ignoring implicit traps? in that case the
    //       benefits are fairly limited, as any trap is an impassable barrier.
    DeadStoreCFG<MemoryLogic>(getModule(), func, getPassOptions()).optimize();

    // Optimize GC heap.
    if (getModule()->features.hasGC()) {
      DeadStoreCFG<GCLogic>(getModule(), func, getPassOptions()).optimize();
    }
  }
};

} // anonymous namespace

Pass* createLocalDeadStoreEliminationPass() {
  return new LocalDeadStoreElimination();
}

} // namespace wasm
