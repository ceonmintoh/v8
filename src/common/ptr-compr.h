// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_H_
#define V8_COMMON_PTR_COMPR_H_

#include "src/base/memory.h"
#include "src/common/globals.h"

namespace v8::internal {

// This is just a collection of compression scheme related functions. Having
// such a class allows plugging different decompression scheme in certain
// places by introducing another CompressionScheme class with a customized
// implementation. This is useful, for example, for CodeDataContainer::code
// field (see CodeObjectSlot).
class V8HeapCompressionScheme {
 public:
  V8_INLINE static Address GetPtrComprCageBaseAddress(Address on_heap_addr);

  V8_INLINE static Address GetPtrComprCageBaseAddress(
      PtrComprCageBase cage_base);

  // Compresses full-pointer representation of a tagged value to on-heap
  // representation.
  V8_INLINE static Tagged_t CompressTagged(Address tagged);

  // Decompresses smi value.
  V8_INLINE static Address DecompressTaggedSigned(Tagged_t raw_value);

  // Decompresses weak or strong heap object pointer or forwarding pointer,
  // preserving both weak- and smi- tags.
  template <typename TOnHeapAddress>
  V8_INLINE static Address DecompressTaggedPointer(TOnHeapAddress on_heap_addr,
                                                   Tagged_t raw_value);
  // Decompresses any tagged value, preserving both weak- and smi- tags.
  template <typename TOnHeapAddress>
  V8_INLINE static Address DecompressTaggedAny(TOnHeapAddress on_heap_addr,
                                               Tagged_t raw_value);

  // Given a 64bit raw value, found on the stack, calls the callback function
  // with all possible pointers that may be "contained" in compressed form in
  // this value, either as complete compressed pointers or as intermediate
  // (half-computed) results.
  template <typename ProcessPointerCallback>
  V8_INLINE static void ProcessIntermediatePointers(
      PtrComprCageBase cage_base, Address raw_value,
      ProcessPointerCallback callback);
};

#ifdef V8_EXTERNAL_CODE_SPACE

// Compression scheme used for fields containing Code objects (namely for the
// CodeDataContainer::code field).
// Unlike the V8HeapCompressionScheme this one allows the cage to cross 4GB
// boundary at a price of making decompression slightly more complex.
// The former outweighs the latter because it gives us more flexibility in
// allocating the code range closer to .text section in the process address
// space. At the same time decompression of the external code field happens
// relatively rarely during GC.
// The base can be any value such that [base, base + 4GB) contains the whole
// code range.
//
// This scheme works as follows:
//    --|----------{---------|------}--------------|--
//     4GB         |        4GB     |             4GB
//                 +-- code range --+
//                 |
//             cage base
//
// * Cage base value is OS page aligned for simplicity (although it's not
//   strictly necessary).
// * Code range size is smaller than or equal to 4GB.
// * Compression is just a truncation to 32-bits value.
// * Decompression of a pointer:
//   - if "compressed" cage base is <= than compressed value then one just
//     needs to OR the upper 32-bits of the case base to get the decompressed
//     value.
//   - if compressed value is smaller than "compressed" cage base then ORing
//     the upper 32-bits of the cage base is not enough because the resulting
//     value will be off by 4GB, which has to be added to the result.
//   - note that decompression doesn't modify the lower 32-bits of the value.
// * Decompression of Smi values is made a no-op for simplicity given that
//   on the hot paths of decompressing the Code pointers it's already known
//   that the value is not a Smi.
//
class ExternalCodeCompressionScheme {
 public:
  V8_INLINE static Address PrepareCageBaseAddress(Address on_heap_addr);

  // Note that this compression scheme doesn't allow reconstruction of the cage
  // base value from any arbitrary value, thus the cage base has to be passed
  // explicitly to the decompression functions.
  static Address GetPtrComprCageBaseAddress(Address on_heap_addr) = delete;

  V8_INLINE static Address GetPtrComprCageBaseAddress(
      PtrComprCageBase cage_base);

  // Compresses full-pointer representation of a tagged value to on-heap
  // representation.
  V8_INLINE static Tagged_t CompressTagged(Address tagged);

  // Decompresses smi value.
  V8_INLINE static Address DecompressTaggedSigned(Tagged_t raw_value);

  // Decompresses weak or strong heap object pointer or forwarding pointer,
  // preserving both weak- and smi- tags.
  template <typename TOnHeapAddress>
  V8_INLINE static Address DecompressTaggedPointer(TOnHeapAddress on_heap_addr,
                                                   Tagged_t raw_value);
  // Decompresses any tagged value, preserving both weak- and smi- tags.
  template <typename TOnHeapAddress>
  V8_INLINE static Address DecompressTaggedAny(TOnHeapAddress on_heap_addr,
                                               Tagged_t raw_value);
};

#endif  // V8_EXTERNAL_CODE_SPACE

// Accessors for fields that may be unaligned due to pointer compression.

template <typename V>
static inline V ReadMaybeUnalignedValue(Address p) {
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
#ifdef V8_COMPRESS_POINTERS
  constexpr bool v8_pointer_compression_unaligned = sizeof(V) > kTaggedSize;
#else
  constexpr bool v8_pointer_compression_unaligned = false;
#endif
  // Bug(v8:8875) Double fields may be unaligned.
  constexpr bool unaligned_double_field =
      std::is_same<V, double>::value && kDoubleSize > kTaggedSize;
  if (unaligned_double_field || v8_pointer_compression_unaligned) {
    return base::ReadUnalignedValue<V>(p);
  } else {
    return base::Memory<V>(p);
  }
}

template <typename V>
static inline void WriteMaybeUnalignedValue(Address p, V value) {
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
#ifdef V8_COMPRESS_POINTERS
  constexpr bool v8_pointer_compression_unaligned = sizeof(V) > kTaggedSize;
#else
  constexpr bool v8_pointer_compression_unaligned = false;
#endif
  // Bug(v8:8875) Double fields may be unaligned.
  constexpr bool unaligned_double_field =
      std::is_same<V, double>::value && kDoubleSize > kTaggedSize;
  if (unaligned_double_field || v8_pointer_compression_unaligned) {
    base::WriteUnalignedValue<V>(p, value);
  } else {
    base::Memory<V>(p) = value;
  }
}

}  // namespace v8::internal

#endif  // V8_COMMON_PTR_COMPR_H_
