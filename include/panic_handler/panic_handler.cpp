/*
 * Copyright (C) 2026 Omega493

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _WIN32
# include <ucontext.h>
#endif // ^^^ !_WIN32 ^^^

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "panic_handler.hpp"

#ifdef _WIN32
LONG WINAPI panic_handler(EXCEPTION_POINTERS* ex_ptrs_) {
  if (!ex_ptrs_) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

# define CTX_REC(name_) static_cast<uint64_t>(ex_ptrs_->ContextRecord->name_)

  static constexpr char REG_DETAILS[]{
  "Registers:\n"
  "RAX: 0x%016llX  RCX: 0x%016llX\n"
  "RDX: 0x%016llX  RBX: 0x%016llX\n"
  "RSP: 0x%016llX  RBP: 0x%016llX\n"
  "RSI: 0x%016llX  RDI: 0x%016llX\n"
  "RIP: 0x%016llX\n"
  "R8:  0x%016llX  R9:  0x%016llX\n"
  "R10: 0x%016llX  R11: 0x%016llX\n"
  "R12: 0x%016llX  R13: 0x%016llX\n"
  "R14: 0x%016llX  R15: 0x%016llX\n"
  };

  // Get the exception code.
  char addr_buf_[240]{};
  const int addr_buf_snprintf_{ std::snprintf(addr_buf_, sizeof(addr_buf_),
    "Process ID: %d\nException at address 0x%016llX: %s\n",
    _getpid(),
    reinterpret_cast<uint64_t>(ex_ptrs_->ExceptionRecord->ExceptionAddress),
    [&ex_ptrs_]() -> const char* {
      switch (ex_ptrs_->ExceptionRecord->ExceptionCode) {
        // General violations
        case EXCEPTION_ACCESS_VIOLATION: {
          return "EXCEPTION_ACCESS_VIOLATION - a thread attempted to read from an address it doesn't have access to.\n";
        }
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: {
          return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED - a thread attempted to access an array element that is out of bounds.\n";
        }
        case EXCEPTION_ILLEGAL_INSTRUCTION: {
          return "EXCEPTION_ILLEGAL_INSTRUCTION - a thread tried executing an illegal instruction.\n";
        }
        case EXCEPTION_IN_PAGE_ERROR: {
          return "EXCEPTION_IN_PAGE_ERROR - a thread tried accessing a page that is not present.\n";
        }
        case EXCEPTION_GUARD_PAGE: {
          return "EXCEPTION_GUARD_PAGE - a thread accessed memory allocated with the PAGE_GUARD modifier.\n";
        }
        case EXCEPTION_INVALID_HANDLE: {
          return "EXCEPTION_INVALID_HANDLE - a thread used a handle to a kernel object that was invalid.\n";
        }
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: {
          return "EXCEPTION_NONCONTINUABLE_EXCEPTION - a thread attempted to continue execution after a non-continuable exception.\n";
        }
        case EXCEPTION_STACK_OVERFLOW: {
          return "EXCEPTION_STACK_OVERFLOW - a thread used up its stack.\n";
        }
        case EXCEPTION_BREAKPOINT: {
          return "EXCEPTION_BREAKPOINT - a breakpoint was encountered (often inserted by MSVC / clang-cl for UB-like nullptr dereference).\n";
        }
        // Floating point exceptions
        case EXCEPTION_FLT_DENORMAL_OPERAND: {
          return "EXCEPTION_FLT_DENORMAL_OPERAND - one of the operands in an floating-point operation is denormal.\n";
        }
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: {
          return "EXCEPTION_FLT_DIVIDE_BY_ZERO - a thread attempted division by zero.\n";
        }
        case EXCEPTION_FLT_OVERFLOW: {
          return "EXCEPTION_FLT_OVERFLOW - the exponent of a floating point operation is greater than the magnitude allowed by its corresponding type.\n";
        }
        case EXCEPTION_FLT_UNDERFLOW: {
          return "EXCEPTION_FLT_UNDERFLOW - the exponent of a floating point operation is smaller than the magnitude allowed by its corresponding type.\n";
        }
        case EXCEPTION_FLT_STACK_CHECK: {
          return "EXCEPTION_FLT_STACK_CHECK - a stack underflow/overflow due to a floating point operation.\n";
        }
        case EXCEPTION_FLT_INVALID_OPERATION: {
          return "EXCEPTION_FLT_INVALID_OPERATION - a general floating point exception.\n";
        }
        // Integer exceptions
        case EXCEPTION_INT_DIVIDE_BY_ZERO: {
          return "EXCEPTION_INT_DIVIDE_BY_ZERO - a thread attempted division by zero.\n";
        }
        case EXCEPTION_INT_OVERFLOW: {
          return "EXCEPTION_INT_OVERFLOW - the result of an integer operation is too large to be held by the destination register.\n";
        }
        default: {
          static char ex_buf_[64]{};
          const int ex_buf_snprintf_{
            std::snprintf(ex_buf_, sizeof(ex_buf_), "ExceptionCode %lu thrown.\n", ex_ptrs_->ExceptionRecord->ExceptionCode)
          };
          return ex_buf_;
        }
      }
    }()
  ) };
  stderr_write(addr_buf_);

  // Get the name of the function that threw.
  const HANDLE process_{ GetCurrentProcess() };

  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  SymInitialize(process_, nullptr, TRUE);

  alignas(SYMBOL_INFO) char symbol_buffer_[sizeof(SYMBOL_INFO) + 256 * sizeof(char)]{};
  SYMBOL_INFO* symbol_{ reinterpret_cast<SYMBOL_INFO*>(symbol_buffer_) };
  symbol_->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol_->MaxNameLen = 255;

  DWORD64 displacement_{};
  char func_name_buf_[300]{};

  if (SymFromAddr(process_, ex_ptrs_->ContextRecord->Rip, &displacement_, symbol_)) {
    const int sz_{ std::snprintf(func_name_buf_, sizeof(func_name_buf_), "Faulting function: %s()\n\n", symbol_->Name) };
    stderr_write(func_name_buf_);
  } else {
    const int sz_{ std::snprintf(func_name_buf_, sizeof(func_name_buf_), "Faulting function: Unknown (maybe pdb not generated?)\n\n") };
    stderr_write(func_name_buf_);
  }

  // Get register dump.
  char reg_dump_buf_[440]{};
  const int reg_dump_buf_snprintf_{ 
    std::snprintf(reg_dump_buf_, sizeof(reg_dump_buf_),
      REG_DETAILS,
      CTX_REC(Rax), CTX_REC(Rcx), CTX_REC(Rdx), CTX_REC(Rbx),
      CTX_REC(Rsp), CTX_REC(Rbp), CTX_REC(Rsi), CTX_REC(Rdi),
      CTX_REC(Rip),
      CTX_REC(R8),  CTX_REC(R9),  CTX_REC(R10), CTX_REC(R11),
      CTX_REC(R12), CTX_REC(R13), CTX_REC(R14), CTX_REC(R15)
    )
  };
  stderr_write(reg_dump_buf_);

  return EXCEPTION_EXECUTE_HANDLER;
}
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
void panic_handler(const int signum_, siginfo_t* siginfo_, void* ctx_ptr_) {
# define CTX_REC(name_) static_cast<uint64_t>(ctx_->uc_mcontext.gregs[name_])

  static constexpr char REG_DETAILS[]{
  "Registers:\n"
  "RAX: 0x%016lX  RCX: 0x%016lX\n"
  "RDX: 0x%016lX  RBX: 0x%016lX\n"
  "RSP: 0x%016lX  RBP: 0x%016lX\n"
  "RSI: 0x%016lX  RDI: 0x%016lX\n"
  "RIP: 0x%016lX\n"
  "R8:  0x%016lX  R9:  0x%016lX\n"
  "R10: 0x%016lX  R11: 0x%016lX\n"
  "R12: 0x%016lX  R13: 0x%016lX\n"
  "R14: 0x%016lX  R15: 0x%016lX\n"
  };

  // Signal info
  char addr_buf_[240]{};
  const int addr_buf_snprintf_{ std::snprintf(addr_buf_, sizeof(addr_buf_),
    "Process ID: %d\nException at address 0x%016lX: %s\n",
    getpid(),
    reinterpret_cast<uint64_t>(siginfo_->si_addr),
    [signum_]() -> const char* {
      switch (signum_) {
        case SIGABRT: {
          return "SIGABRT - abnormal termination request, maybe std::abort().\n";
        }
        case SIGFPE: {
          return "SIGFPE - floating point exception (erroneous arithmatic operation, maybe division by zero).\n";
        }
        case SIGILL: {
          return "SIGILL - illegal instruction (invalid program image).\n";
        }
        case SIGSEGV: {
          return "SIGSEGV - segment violation.\n";
        }
        default: {
          static char ex_buf_[64]{};
          const int ex_buf_snprintf_{ std::snprintf(ex_buf_, sizeof(ex_buf_), "Signal number %d thrown.\n", signum_) };
          return ex_buf_;
        }
      }
    }()
  ) };
  stderr_write(addr_buf_);
  
  if (!ctx_ptr_) {
    std::_Exit(signum_);
  }

  ucontext_t* ctx_{ static_cast<ucontext_t*>(ctx_ptr_) };

  if (!ctx_) {
    std::_Exit(signum_);
  }

  // Get function info.
  Dl_info info_{};
  char func_name_buf_[300]{};
  const void* rip_address_{ reinterpret_cast<void*>(CTX_REC(REG_RIP)) };

  if (dladdr(rip_address_, &info_)) {
    const char* func_name_{ info_.dli_sname ? info_.dli_sname : "Unknown" };
    const int sz_{ std::snprintf(func_name_buf_, sizeof(func_name_buf_), "Faulting function: %s()\n\n", func_name_) };
    stderr_write(func_name_buf_);
  } else {
    const int sz_{ std::snprintf(func_name_buf_, sizeof(func_name_buf_), "Faulting function: Unknown (maybe pdb not generated?)\n\n") };
    stderr_write(func_name_buf_);
  }

  // Register details.
  char reg_dump_buf_[440]{};
  const int reg_dump_buf_snprintf_{
    std::snprintf(reg_dump_buf_, sizeof(reg_dump_buf_),
      REG_DETAILS,
      CTX_REC(REG_RAX), CTX_REC(REG_RCX), CTX_REC(REG_RDX), CTX_REC(REG_RBX),
      CTX_REC(REG_RSP), CTX_REC(REG_RBP), CTX_REC(REG_RSI), CTX_REC(REG_RDI),
      CTX_REC(REG_RIP),
      CTX_REC(REG_R8),  CTX_REC(REG_R9),  CTX_REC(REG_R10), CTX_REC(REG_R11),
      CTX_REC(REG_R12), CTX_REC(REG_R13), CTX_REC(REG_R14), CTX_REC(REG_R15)
    )
  };
  stderr_write(reg_dump_buf_);

  std::_Exit(signum_);
}
#endif // ^^^ _WIN32 ^^^
