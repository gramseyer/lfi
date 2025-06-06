#include "lfi_arch.h"
#include "pal/platform.h"

EXPORT void
lfi_regs_init(struct TuxRegs* regs, struct LFIAddrSpace* as, struct LFIContext* ctx)
{
    regs->r14 = as->base;
    regs->gs  = as->base;
    regs->rsp = as->base;
    regs->r15 = 0xffffffffffffffff;
    regs->r13 = (uintptr_t) ctx->sys;
}

EXPORT void
lfi_ctx_init_sys(struct LFIContext* ctx)
{
    // sys-external not yet supported on x64
}

uintptr_t*
lfi_regs_entry(struct TuxRegs* regs)
{
    return &regs->r11;
}

uintptr_t*
lfi_regs_arg0(struct TuxRegs* regs)
{
    return &regs->rdi;
}
