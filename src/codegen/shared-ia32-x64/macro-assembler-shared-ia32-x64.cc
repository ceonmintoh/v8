// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h"

#include "src/codegen/assembler.h"
#include "src/codegen/cpu-features.h"

#if V8_TARGET_ARCH_IA32
#include "src/codegen/ia32/register-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/codegen/x64/register-x64.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

void SharedTurboAssembler::I16x8ExtMulHighS(XMMRegister dst, XMMRegister src1,
                                            XMMRegister src2,
                                            XMMRegister scratch) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpunpckhbw(scratch, src1, src1);
    vpsraw(scratch, scratch, 8);
    vpunpckhbw(dst, src2, src2);
    vpsraw(dst, dst, 8);
    vpmullw(dst, dst, scratch);
  } else {
    if (dst != src1) {
      movaps(dst, src1);
    }
    movaps(scratch, src2);
    punpckhbw(dst, dst);
    psraw(dst, 8);
    punpckhbw(scratch, scratch);
    psraw(scratch, 8);
    pmullw(dst, scratch);
  }
}

void SharedTurboAssembler::I16x8ExtMulHighU(XMMRegister dst, XMMRegister src1,
                                            XMMRegister src2,
                                            XMMRegister scratch) {
  // The logic here is slightly complicated to handle all the cases of register
  // aliasing. This allows flexibility for callers in TurboFan and Liftoff.
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    if (src1 == src2) {
      vpxor(scratch, scratch, scratch);
      vpunpckhbw(dst, src1, scratch);
      vpmullw(dst, dst, dst);
    } else {
      if (dst == src2) {
        // We overwrite dst, then use src2, so swap src1 and src2.
        std::swap(src1, src2);
      }
      vpxor(scratch, scratch, scratch);
      vpunpckhbw(dst, src1, scratch);
      vpunpckhbw(scratch, src2, scratch);
      vpmullw(dst, dst, scratch);
    }
  } else {
    if (src1 == src2) {
      xorps(scratch, scratch);
      if (dst != src1) {
        movaps(dst, src1);
      }
      punpckhbw(dst, scratch);
      pmullw(dst, scratch);
    } else {
      // When dst == src1, nothing special needs to be done.
      // When dst == src2, swap src1 and src2, since we overwrite dst.
      // When dst is unique, copy src1 to dst first.
      if (dst == src2) {
        std::swap(src1, src2);
        // Now, dst == src1.
      } else if (dst != src1) {
        // dst != src1 && dst != src2.
        movaps(dst, src1);
      }
      xorps(scratch, scratch);
      punpckhbw(dst, scratch);
      punpckhbw(scratch, src2);
      psrlw(scratch, 8);
      pmullw(dst, scratch);
    }
  }
}

void SharedTurboAssembler::I16x8SConvertI8x16High(XMMRegister dst,
                                                  XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    // src = |a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p| (high)
    // dst = |i|i|j|j|k|k|l|l|m|m|n|n|o|o|p|p|
    vpunpckhbw(dst, src, src);
    vpsraw(dst, dst, 8);
  } else {
    CpuFeatureScope sse_scope(this, SSE4_1);
    if (dst == src) {
      // 2 bytes shorter than pshufd, but has depdency on dst.
      movhlps(dst, src);
      pmovsxbw(dst, dst);
    } else {
      // No dependency on dst.
      pshufd(dst, src, 0xEE);
      pmovsxbw(dst, dst);
    }
  }
}

void SharedTurboAssembler::I16x8UConvertI8x16High(XMMRegister dst,
                                                  XMMRegister src,
                                                  XMMRegister scratch) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    // tmp = |0|0|0|0|0|0|0|0 | 0|0|0|0|0|0|0|0|
    // src = |a|b|c|d|e|f|g|h | i|j|k|l|m|n|o|p|
    // dst = |0|a|0|b|0|c|0|d | 0|e|0|f|0|g|0|h|
    XMMRegister tmp = dst == src ? scratch : dst;
    vpxor(tmp, tmp, tmp);
    vpunpckhbw(dst, src, tmp);
  } else {
    CpuFeatureScope sse_scope(this, SSE4_1);
    if (dst == src) {
      // xorps can be executed on more ports than pshufd.
      xorps(scratch, scratch);
      punpckhbw(dst, scratch);
    } else {
      // No dependency on dst.
      pshufd(dst, src, 0xEE);
      pmovzxbw(dst, dst);
    }
  }
}

// 1. Multiply low word into scratch.
// 2. Multiply high word (can be signed or unsigned) into dst.
// 3. Unpack and interleave scratch and dst into dst.
void SharedTurboAssembler::I32x4ExtMul(XMMRegister dst, XMMRegister src1,
                                       XMMRegister src2, XMMRegister scratch,
                                       bool low, bool is_signed) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpmullw(scratch, src1, src2);
    is_signed ? vpmulhw(dst, src1, src2) : vpmulhuw(dst, src1, src2);
    low ? vpunpcklwd(dst, scratch, dst) : vpunpckhwd(dst, scratch, dst);
  } else {
    DCHECK_EQ(dst, src1);
    movaps(scratch, src1);
    pmullw(dst, src2);
    is_signed ? pmulhw(scratch, src2) : pmulhuw(scratch, src2);
    low ? punpcklwd(dst, scratch) : punpckhwd(dst, scratch);
  }
}

void SharedTurboAssembler::I32x4SConvertI16x8High(XMMRegister dst,
                                                  XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    // src = |a|b|c|d|e|f|g|h| (high)
    // dst = |e|e|f|f|g|g|h|h|
    vpunpckhwd(dst, src, src);
    vpsrad(dst, dst, 16);
  } else {
    CpuFeatureScope sse_scope(this, SSE4_1);
    if (dst == src) {
      // 2 bytes shorter than pshufd, but has depdency on dst.
      movhlps(dst, src);
      pmovsxwd(dst, dst);
    } else {
      // No dependency on dst.
      pshufd(dst, src, 0xEE);
      pmovsxwd(dst, dst);
    }
  }
}

void SharedTurboAssembler::I32x4UConvertI16x8High(XMMRegister dst,
                                                  XMMRegister src,
                                                  XMMRegister scratch) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    // scratch = |0|0|0|0|0|0|0|0|
    // src     = |a|b|c|d|e|f|g|h|
    // dst     = |0|a|0|b|0|c|0|d|
    XMMRegister tmp = dst == src ? scratch : dst;
    vpxor(tmp, tmp, tmp);
    vpunpckhwd(dst, src, tmp);
  } else {
    if (dst == src) {
      // xorps can be executed on more ports than pshufd.
      xorps(scratch, scratch);
      punpckhwd(dst, scratch);
    } else {
      CpuFeatureScope sse_scope(this, SSE4_1);
      // No dependency on dst.
      pshufd(dst, src, 0xEE);
      pmovzxwd(dst, dst);
    }
  }
}

// 1. Unpack src0, src1 into even-number elements of scratch.
// 2. Unpack src1, src0 into even-number elements of dst.
// 3. Multiply 1. with 2.
// For non-AVX, use non-destructive pshufd instead of punpckldq/punpckhdq.
void SharedTurboAssembler::I64x2ExtMul(XMMRegister dst, XMMRegister src1,
                                       XMMRegister src2, XMMRegister scratch,
                                       bool low, bool is_signed) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    if (low) {
      vpunpckldq(scratch, src1, src1);
      vpunpckldq(dst, src2, src2);
    } else {
      vpunpckhdq(scratch, src1, src1);
      vpunpckhdq(dst, src2, src2);
    }
    if (is_signed) {
      vpmuldq(dst, scratch, dst);
    } else {
      vpmuludq(dst, scratch, dst);
    }
  } else {
    uint8_t mask = low ? 0x50 : 0xFA;
    pshufd(scratch, src1, mask);
    pshufd(dst, src2, mask);
    if (is_signed) {
      CpuFeatureScope sse4_scope(this, SSE4_1);
      pmuldq(dst, scratch);
    } else {
      pmuludq(dst, scratch);
    }
  }
}

void SharedTurboAssembler::I64x2SConvertI32x4High(XMMRegister dst,
                                                  XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpunpckhqdq(dst, src, src);
    vpmovsxdq(dst, dst);
  } else {
    CpuFeatureScope sse_scope(this, SSE4_1);
    if (dst == src) {
      movhlps(dst, src);
    } else {
      pshufd(dst, src, 0xEE);
    }
    pmovsxdq(dst, dst);
  }
}

void SharedTurboAssembler::I64x2UConvertI32x4High(XMMRegister dst,
                                                  XMMRegister src,
                                                  XMMRegister scratch) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope avx_scope(this, AVX);
    vpxor(scratch, scratch, scratch);
    vpunpckhdq(dst, src, scratch);
  } else {
    if (dst != src) {
      movaps(dst, src);
    }
    xorps(scratch, scratch);
    punpckhdq(dst, scratch);
  }
}

}  // namespace internal
}  // namespace v8