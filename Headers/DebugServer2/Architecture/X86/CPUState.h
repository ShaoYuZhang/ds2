//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Architecture_X86_CPUState_h
#define __DebugServer2_Architecture_X86_CPUState_h

#include "DebugServer2/Architecture/X86/RegistersDescriptors.h"

#include <cstring>

namespace ds2 {
namespace Architecture {
namespace X86 {

#pragma pack(push, 1)

struct SSEVector {
  uint64_t value[2]; // 128-bit values
};

struct AVXVector {
  uint64_t value[4]; // 256-bit values
};

struct X87Register {
  union {
    long double f80;
    double f64;
    float f32;
    uint64_t mm;
    uint8_t bytes[10];
  };
};

struct CPUState {
  union {
    uint32_t regs[16];
    struct {
      uint32_t eax, ecx, edx, ebx, esi, edi, esp, ebp, eip, cs, ss, ds, es, fs,
          gs, eflags;
    };
  } gp;

  struct {
    X87Register regs[8];
    uint32_t fstw;
    uint32_t fctw;
    uint32_t ftag;
    uint32_t fiseg;
    uint32_t fioff;
    uint32_t foseg;
    uint32_t fooff;
    uint32_t fop;
  } x87;

  union {
    struct {
      uint32_t mxcsr;
      uint32_t mxcsrmask;
      struct {
        // Dirty hack to map AVX register locations
        union {
          AVXVector _avx[8];
          SSEVector _sse[16];
        };
        SSEVector const &operator[](size_t index) const {
          return _sse[index << 1];
        }
        SSEVector &operator[](size_t index) { return _sse[index << 1]; }
      } regs;
    } sse;

    struct {
      uint32_t mxcsr;
      uint32_t mxcsrmask;
      AVXVector regs[8];
    } avx;
  };

  struct {
    uint32_t dr[8];
  } dr;

#if defined(__linux__)
  struct {
    uint32_t orig_eax;
  } linux_gp;
#endif

public:
  CPUState() { clear(); }

  inline void clear() {
    std::memset(&gp, 0, sizeof(gp));
    std::memset(&x87, 0, sizeof(x87));
    std::memset(&avx, 0, sizeof(avx));
    std::memset(&dr, 0, sizeof(dr));
#if defined(__linux__)
    std::memset(&linux_gp, 0, sizeof(linux_gp));
#endif
  }

public:
  //
  // Accessors
  //
  inline uint32_t pc() const { return gp.eip; }
  inline void setPC(uint32_t pc) { gp.eip = pc; }

  inline uint32_t xpc() const { return gp.eip; }

  inline uint32_t sp() const { return gp.esp; }
  inline void setSP(uint32_t sp) { gp.esp = sp; }

  inline uint32_t retval() const { return gp.eax; }

public:
#define _REGVALUE(REG)                                                         \
  GPRegisterValue { sizeof(gp.REG), gp.REG }
  //
  // These two functions interprets regs as GDB packed registers,
  // the order of GPR state, which is NOT the same order as the reg_gdb_*
  // (*sigh*)
  // is as follow:
  //
  // eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cs, ss, ds, es, fs, gs
  //
  inline void getGPState(GPRegisterValueVector &regs) const {
    regs.clear();
    regs.push_back(_REGVALUE(eax));
    regs.push_back(_REGVALUE(ebx));
    regs.push_back(_REGVALUE(ecx));
    regs.push_back(_REGVALUE(edx));
    regs.push_back(_REGVALUE(esi));
    regs.push_back(_REGVALUE(edi));
    regs.push_back(_REGVALUE(ebp));
    regs.push_back(_REGVALUE(esp));
    regs.push_back(_REGVALUE(eip));
    regs.push_back(_REGVALUE(eflags));
    regs.push_back(_REGVALUE(cs));
    regs.push_back(_REGVALUE(ss));
    regs.push_back(_REGVALUE(ds));
    regs.push_back(_REGVALUE(es));
    regs.push_back(_REGVALUE(fs));
    regs.push_back(_REGVALUE(gs));
  }

  inline void setGPState(std::vector<uint64_t> const &regs) {
    gp.eax = regs[0];
    gp.ebx = regs[1];
    gp.ecx = regs[2];
    gp.edx = regs[3];
    gp.esi = regs[4];
    gp.edi = regs[5];
    gp.ebp = regs[6];
    gp.esp = regs[7];
    gp.eip = regs[8];
    gp.eflags = regs[9];
    gp.cs = regs[10];
    gp.ss = regs[11];
    gp.ds = regs[12];
    gp.es = regs[13];
    gp.fs = regs[14];
    gp.gs = regs[15];
  }

public:
  inline void getStopGPState(GPRegisterStopMap &regs, bool forLLDB) const {
    if (forLLDB) {
#define _SETREG(REG) regs[reg_lldb_##REG] = _REGVALUE(REG)
      _SETREG(eax);
      _SETREG(ebx);
      _SETREG(ecx);
      _SETREG(edx);
      _SETREG(ebx);
      _SETREG(esi);
      _SETREG(edi);
      _SETREG(ebp);
      _SETREG(esp);
      _SETREG(eip);
      _SETREG(cs);
      _SETREG(ss);
      _SETREG(ds);
      _SETREG(es);
      _SETREG(fs);
      _SETREG(gs);
      _SETREG(eflags);
#undef _SETREG
    } else {
#define _SETREG(REG) regs[reg_gdb_##REG] = _REGVALUE(REG)
      _SETREG(eax);
      _SETREG(ebx);
      _SETREG(ecx);
      _SETREG(edx);
      _SETREG(ebx);
      _SETREG(esi);
      _SETREG(edi);
      _SETREG(ebp);
      _SETREG(esp);
      _SETREG(eip);
      _SETREG(cs);
      _SETREG(ss);
      _SETREG(ds);
      _SETREG(es);
      _SETREG(fs);
      _SETREG(gs);
      _SETREG(eflags);
#undef _SETREG
    }
  }
#undef _REGVALUE

public:
  inline bool getLLDBRegisterPtr(int regno, void **ptr, size_t *length) const {
#define _GETREG2(T, REG, FLD)                                                  \
  case reg_lldb_##REG:                                                         \
    *ptr = &const_cast<CPUState *>(this)->T.FLD, *length = sizeof(T.FLD);      \
    break
#define _GETREG8L(T, REG, FREG)                                                \
  case reg_lldb_##REG:                                                         \
    *ptr = reinterpret_cast<uint8_t *>(&const_cast<CPUState *>(this)->T.FREG), \
    *length = sizeof(uint8_t);                                                 \
    break
#define _GETREG8H(T, REG, FREG)                                                \
  case reg_lldb_##REG:                                                         \
    *ptr =                                                                     \
        reinterpret_cast<uint8_t *>(&const_cast<CPUState *>(this)->T.FREG) +   \
        1,                                                                     \
    *length = sizeof(uint8_t);                                                 \
    break
#define _GETREG16(T, REG, FREG)                                                \
  case reg_lldb_##REG:                                                         \
    *ptr =                                                                     \
        reinterpret_cast<uint16_t *>(&const_cast<CPUState *>(this)->T.FREG),   \
    *length = sizeof(uint16_t);                                                \
    break
#define _GETREG(T, REG) _GETREG2(T, REG, REG)

    switch (regno) {
      _GETREG(gp, eax);
      _GETREG(gp, ebx);
      _GETREG(gp, ecx);
      _GETREG(gp, edx);
      _GETREG(gp, esi);
      _GETREG(gp, edi);
      _GETREG(gp, esp);
      _GETREG(gp, ebp);
      _GETREG(gp, eip);
      _GETREG(gp, cs);
      _GETREG(gp, ss);
      _GETREG(gp, ds);
      _GETREG(gp, es);
      _GETREG(gp, fs);
      _GETREG(gp, gs);
      _GETREG(gp, eflags);

      _GETREG16(gp, ax, eax);
      _GETREG16(gp, bx, ebx);
      _GETREG16(gp, cx, ecx);
      _GETREG16(gp, dx, edx);
      _GETREG16(gp, si, esi);
      _GETREG16(gp, di, edi);
      _GETREG16(gp, sp, esp);
      _GETREG16(gp, bp, ebp);

      _GETREG8L(gp, al, eax);
      _GETREG8L(gp, bl, ebx);
      _GETREG8L(gp, cl, ecx);
      _GETREG8L(gp, dl, edx);

      _GETREG8H(gp, ah, eax);
      _GETREG8H(gp, bh, ebx);
      _GETREG8H(gp, ch, ecx);
      _GETREG8H(gp, dh, edx);

      _GETREG2(x87, st0, regs[0].bytes);
      _GETREG2(x87, st1, regs[1].bytes);
      _GETREG2(x87, st2, regs[2].bytes);
      _GETREG2(x87, st3, regs[3].bytes);
      _GETREG2(x87, st4, regs[4].bytes);
      _GETREG2(x87, st5, regs[5].bytes);
      _GETREG2(x87, st6, regs[6].bytes);
      _GETREG2(x87, st7, regs[7].bytes);
      _GETREG2(x87, fstat, fstw);
      _GETREG2(x87, fctrl, fctw);
      _GETREG(x87, ftag);
      _GETREG(x87, fiseg);
      _GETREG(x87, fioff);
      _GETREG(x87, foseg);
      _GETREG(x87, fooff);
      _GETREG(x87, fop);

      _GETREG(avx, mxcsr);
      _GETREG(avx, mxcsrmask);
      _GETREG2(avx, ymm0, regs[0]);
      _GETREG2(avx, ymm1, regs[1]);
      _GETREG2(avx, ymm2, regs[2]);
      _GETREG2(avx, ymm3, regs[3]);
      _GETREG2(avx, ymm4, regs[4]);
      _GETREG2(avx, ymm5, regs[5]);
      _GETREG2(avx, ymm6, regs[6]);
      _GETREG2(avx, ymm7, regs[7]);

    default:
      return false;
    }
#undef _GETREG16
#undef _GETREG8H
#undef _GETREG8L
#undef _GETREG
#undef _GETREG2

    return true;
  }

  inline bool getGDBRegisterPtr(int regno, void **ptr, size_t *length) const {
#define _GETREG2(T, REG, FLD)                                                  \
  case reg_gdb_##REG:                                                          \
    *ptr = &const_cast<CPUState *>(this)->T.FLD, *length = sizeof(T.FLD);      \
    break
#define _GETREG(T, REG) _GETREG2(T, REG, REG)

    switch (regno) {
      _GETREG(gp, eax);
      _GETREG(gp, ebx);
      _GETREG(gp, ecx);
      _GETREG(gp, edx);
      _GETREG(gp, esi);
      _GETREG(gp, edi);
      _GETREG(gp, esp);
      _GETREG(gp, ebp);
      _GETREG(gp, eip);
      _GETREG(gp, cs);
      _GETREG(gp, ss);
      _GETREG(gp, ds);
      _GETREG(gp, es);
      _GETREG(gp, fs);
      _GETREG(gp, gs);
      _GETREG(gp, eflags);

      _GETREG2(x87, st0, regs[0].bytes);
      _GETREG2(x87, st1, regs[1].bytes);
      _GETREG2(x87, st2, regs[2].bytes);
      _GETREG2(x87, st3, regs[3].bytes);
      _GETREG2(x87, st4, regs[4].bytes);
      _GETREG2(x87, st5, regs[5].bytes);
      _GETREG2(x87, st6, regs[6].bytes);
      _GETREG2(x87, st7, regs[7].bytes);
      _GETREG2(x87, fstat, fstw);
      _GETREG2(x87, fctrl, fctw);
      _GETREG(x87, ftag);
      _GETREG(x87, fiseg);
      _GETREG(x87, fioff);
      _GETREG(x87, foseg);
      _GETREG(x87, fooff);
      _GETREG(x87, fop);

      // ymm0 maps to xmm0 for gdb
      _GETREG2(sse, ymm0, regs[0]);
      _GETREG2(sse, ymm1, regs[1]);
      _GETREG2(sse, ymm2, regs[2]);
      _GETREG2(sse, ymm3, regs[3]);
      _GETREG2(sse, ymm4, regs[4]);
      _GETREG2(sse, ymm5, regs[5]);
      _GETREG2(sse, ymm6, regs[6]);
      _GETREG2(sse, ymm7, regs[7]);

      _GETREG(sse, mxcsr);

#if defined(__linux__)
      _GETREG(linux_gp, orig_eax);
#endif

    default:
      return false;
    }
#undef _GETREG
#undef _GETREG2

    return true;
  }
};

#pragma pack(pop)
}
}
}

#endif // !__DebugServer2_Architecture_X86_CPUState_h
