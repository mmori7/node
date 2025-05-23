// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
#define V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <unordered_map>

#include "src/base/platform/mutex.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8::internal::wasm {

class WasmCode;
class WasmEngine;

// Implements a cache for import wrappers.
class WasmImportWrapperCache {
 public:
  struct CacheKey {
    CacheKey(ImportCallKind kind, CanonicalTypeIndex type_index,
             int expected_arity, Suspend suspend)
        : kind(kind),
          type_index(type_index),
          expected_arity(expected_arity),
          suspend(suspend) {}

    bool operator==(const CacheKey& rhs) const = default;

    ImportCallKind kind;
    CanonicalTypeIndex type_index;
    int expected_arity;
    Suspend suspend;
  };

  class CacheKeyHash {
   public:
    size_t operator()(const CacheKey& key) const {
      return base::hash_combine(static_cast<uint8_t>(key.kind),
                                key.type_index.index, key.expected_arity);
    }
  };

  // Helper class to modify the cache under a lock.
  class V8_NODISCARD ModificationScope {
   public:
    explicit ModificationScope(WasmImportWrapperCache* cache)
        : cache_(cache), guard_(&cache->mutex_) {}

    V8_EXPORT_PRIVATE WasmCode* operator[](const CacheKey& key);

    WasmCode* AddWrapper(const CacheKey& key, WasmCompilationResult result,
                         WasmCode::Kind kind, uint64_t signature_hash);

   private:
    WasmImportWrapperCache* const cache_;
    base::MutexGuard guard_;
  };

  WasmImportWrapperCache() = default;
  ~WasmImportWrapperCache() = default;

  void LazyInitialize(Isolate* triggering_isolate);

  void Free(std::vector<WasmCode*>& wrappers);

  // Thread-safe. Returns nullptr if the key doesn't exist in the map.
  // Adds the returned code to the surrounding WasmCodeRefScope.
  V8_EXPORT_PRIVATE WasmCode* MaybeGet(ImportCallKind kind,
                                       CanonicalTypeIndex type_index,
                                       int expected_arity,
                                       Suspend suspend) const;

  WasmCode* Lookup(Address pc) const;

  void LogForIsolate(Isolate* isolate);

  size_t EstimateCurrentMemoryConsumption() const;

  // Returns nullptr if {call_target} doesn't belong to a known wrapper.
  V8_EXPORT_PRIVATE WasmCode* FindWrapper(WasmCodePointer call_target);

  WasmCode* CompileWasmImportCallWrapper(Isolate* isolate, ImportCallKind kind,
                                         const CanonicalSig* sig,
                                         CanonicalTypeIndex sig_index,
                                         bool source_positions,
                                         int expected_arity, Suspend suspend);

 private:
  std::unique_ptr<WasmCodeAllocator> code_allocator_;
  mutable base::Mutex mutex_;
  std::unordered_map<CacheKey, WasmCode*, CacheKeyHash> entry_map_;
  // Lookup support. The map key is the instruction start address.
  std::map<Address, WasmCode*> codes_;
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
