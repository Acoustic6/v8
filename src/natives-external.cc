// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/natives.h"

#include "src/checks.h"
#include "src/list.h"
#include "src/list-inl.h"
#include "src/snapshot-source-sink.h"
#include "src/vector.h"

namespace v8 {
namespace internal {


/**
 * NativesStore stores the 'native' (builtin) JS libraries.
 *
 * NativesStore needs to be initialized before using V8, usually by the
 * embedder calling v8::SetNativesDataBlob, which calls SetNativesFromFile
 * below.
 */
class NativesStore {
 public:
  ~NativesStore() {}

  int GetBuiltinsCount() { return native_names_.length(); }
  int GetDebuggerCount() { return debugger_count_; }
  Vector<const char> GetScriptName(int index) { return native_names_[index]; }
  Vector<const char> GetRawScriptSource(int index) {
    return native_source_[index];
  }

  int GetIndex(const char* name) {
    for (int i = 0; i < native_names_.length(); ++i) {
      if (strcmp(name, native_names_[i].start()) == 0) {
        return i;
      }
    }
    ASSERT(false);
    return -1;
  }

  int GetRawScriptsSize() {
    ASSERT(false);  // Used for compression. Doesn't really make sense here.
    return 0;
  }

  Vector<const byte> GetScriptsSource() {
    ASSERT(false);  // Used for compression. Doesn't really make sense here.
    return Vector<const byte>();
  }

  static NativesStore* MakeFromScriptsSource(SnapshotByteSource* source) {
    NativesStore* store = new NativesStore;

    // We expect the libraries in the following format:
    //   int: # of debugger sources.
    //   2N blobs: N pairs of source name + actual source.
    //   then, repeat for non-debugger sources.
    int debugger_count = source->GetInt();
    for (int i = 0; i < debugger_count; ++i)
      store->ReadNameAndContentPair(source);
    int library_count = source->GetInt();
    for (int i = 0; i < library_count; ++i)
      store->ReadNameAndContentPair(source);

    store->debugger_count_ = debugger_count;
    return store;
  }

 private:
  NativesStore() : debugger_count_(0) {}

  bool ReadNameAndContentPair(SnapshotByteSource* bytes) {
    const byte* name;
    int name_length;
    const byte* source;
    int source_length;
    bool success = bytes->GetBlob(&name, &name_length) &&
                   bytes->GetBlob(&source, &source_length);
    if (success) {
      Vector<const char> name_vector(
          reinterpret_cast<const char*>(name), name_length);
      Vector<const char> source_vector(
          reinterpret_cast<const char*>(source), source_length);
      native_names_.Add(name_vector);
      native_source_.Add(source_vector);
    }
    return success;
  }

  List<Vector<const char> > native_names_;
  List<Vector<const char> > native_source_;
  int debugger_count_;

  DISALLOW_COPY_AND_ASSIGN(NativesStore);
};


template<NativeType type>
class NativesHolder {
 public:
  static NativesStore* get() {
    ASSERT(holder_);
    return holder_;
  }
  static void set(NativesStore* store) {
    ASSERT(store);
    holder_ = store;
  }

 private:
  static NativesStore* holder_;
};

template<NativeType type>
NativesStore* NativesHolder<type>::holder_ = NULL;


/**
 * Read the Natives (library sources) blob, as generated by js2c + the build
 * system.
 */
void SetNativesFromFile(StartupData* natives_blob) {
  ASSERT(natives_blob);
  ASSERT(natives_blob->data);
  ASSERT(natives_blob->raw_size > 0);

  SnapshotByteSource bytes(
      reinterpret_cast<const byte*>(natives_blob->data),
      natives_blob->raw_size);
  NativesHolder<CORE>::set(NativesStore::MakeFromScriptsSource(&bytes));
  NativesHolder<EXPERIMENTAL>::set(NativesStore::MakeFromScriptsSource(&bytes));
  ASSERT(!bytes.HasMore());
}


// Implement NativesCollection<T> bsaed on NativesHolder + NativesStore.
//
// (The callers expect a purely static interface, since this is how the
//  natives are usually compiled in. Since we implement them based on
//  runtime content, we have to implement this indirection to offer
//  a static interface.)
template<NativeType type>
int NativesCollection<type>::GetBuiltinsCount() {
  return NativesHolder<type>::get()->GetBuiltinsCount();
}

template<NativeType type>
int NativesCollection<type>::GetDebuggerCount() {
  return NativesHolder<type>::get()->GetDebuggerCount();
}

template<NativeType type>
int NativesCollection<type>::GetIndex(const char* name) {
  return NativesHolder<type>::get()->GetIndex(name);
}

template<NativeType type>
int NativesCollection<type>::GetRawScriptsSize() {
  return NativesHolder<type>::get()->GetRawScriptsSize();
}

template<NativeType type>
Vector<const char> NativesCollection<type>::GetRawScriptSource(int index) {
  return NativesHolder<type>::get()->GetRawScriptSource(index);
}

template<NativeType type>
Vector<const char> NativesCollection<type>::GetScriptName(int index) {
  return NativesHolder<type>::get()->GetScriptName(index);
}

template<NativeType type>
Vector<const byte> NativesCollection<type>::GetScriptsSource() {
  return NativesHolder<type>::get()->GetScriptsSource();
}

template<NativeType type>
void NativesCollection<type>::SetRawScriptsSource(
    Vector<const char> raw_source) {
  CHECK(false);  // Use SetNativesFromFile for this implementation.
}


// The compiler can't 'see' all uses of the static methods and hence
// my chose to elide them. This we'll explicitly instantiate these.
template class NativesCollection<CORE>;
template class NativesCollection<EXPERIMENTAL>;
template class NativesCollection<D8>;
template class NativesCollection<TEST>;

}  // namespace v8::internal
}  // namespace v8
