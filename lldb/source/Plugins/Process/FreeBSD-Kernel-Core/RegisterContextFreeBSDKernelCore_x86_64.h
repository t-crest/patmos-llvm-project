//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

<<<<<<<< HEAD:lldb/source/Plugins/Process/FreeBSD-Kernel-Core/RegisterContextFreeBSDKernelCore_x86_64.h
#ifndef LLDB_SOURCE_PLUGINS_PROCESS_FREEBSDKERNEL_REGISTERCONTEXTFREEBSDKERNELCORE_X86_64_H
#define LLDB_SOURCE_PLUGINS_PROCESS_FREEBSDKERNEL_REGISTERCONTEXTFREEBSDKERNELCORE_X86_64_H
========
#ifndef LLDB_SOURCE_PLUGINS_PROCESS_FREEBSDKERNEL_REGISTERCONTEXTFREEBSDKERNELCORE_I386_H
#define LLDB_SOURCE_PLUGINS_PROCESS_FREEBSDKERNEL_REGISTERCONTEXTFREEBSDKERNELCORE_I386_H
>>>>>>>> 555af43a2ad1 (Merge remote-tracking branch 'rust-upstream/rustc/23.1-2026-07-22' into patmos-llvm-23x-rust):lldb/source/Plugins/Process/FreeBSD-Kernel-Core/RegisterContextFreeBSDKernelCore_i386.h

#include "Plugins/Process/Utility/RegisterContextPOSIX_x86.h"
#include "Plugins/Process/elf-core/RegisterUtilities.h"

<<<<<<<< HEAD:lldb/source/Plugins/Process/FreeBSD-Kernel-Core/RegisterContextFreeBSDKernelCore_x86_64.h
class RegisterContextFreeBSDKernelCore_x86_64
    : public RegisterContextPOSIX_x86 {
public:
  RegisterContextFreeBSDKernelCore_x86_64(
========
class RegisterContextFreeBSDKernelCore_i386 : public RegisterContextPOSIX_x86 {
public:
  RegisterContextFreeBSDKernelCore_i386(
>>>>>>>> 555af43a2ad1 (Merge remote-tracking branch 'rust-upstream/rustc/23.1-2026-07-22' into patmos-llvm-23x-rust):lldb/source/Plugins/Process/FreeBSD-Kernel-Core/RegisterContextFreeBSDKernelCore_i386.h
      lldb_private::Thread &thread,
      lldb_private::RegisterInfoInterface *register_info,
      lldb::addr_t pcb_addr);

  bool ReadRegister(const lldb_private::RegisterInfo *reg_info,
                    lldb_private::RegisterValue &value) override;

  bool WriteRegister(const lldb_private::RegisterInfo *reg_info,
                     const lldb_private::RegisterValue &value) override;

protected:
  bool ReadGPR() override;

  bool ReadFPR() override;

  bool WriteGPR() override;

  bool WriteFPR() override;

private:
  lldb::addr_t m_pcb_addr;
};

<<<<<<<< HEAD:lldb/source/Plugins/Process/FreeBSD-Kernel-Core/RegisterContextFreeBSDKernelCore_x86_64.h
#endif // LLDB_SOURCE_PLUGINS_PROCESS_FREEBSDKERNEL_REGISTERCONTEXTFREEBSDKERNELCORE_X86_64_H
========
#endif // LLDB_SOURCE_PLUGINS_PROCESS_FREEBSDKERNEL_REGISTERCONTEXTFREEBSDKERNELCORE_I386_H
>>>>>>>> 555af43a2ad1 (Merge remote-tracking branch 'rust-upstream/rustc/23.1-2026-07-22' into patmos-llvm-23x-rust):lldb/source/Plugins/Process/FreeBSD-Kernel-Core/RegisterContextFreeBSDKernelCore_i386.h
