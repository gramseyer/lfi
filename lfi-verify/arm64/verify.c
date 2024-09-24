#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#include "lfiv.h"
#include "disarm64.h"
#include "verifier.h"

enum {
    ERRMAX = 128, // maximum size for error
};

static void
verrmin(Verifier* v, const char* fmt, ...)
{
    v->failed = true;

    if (!v->err)
        return;

    va_list ap;

    char errbuf[ERRMAX];

    va_start(ap, fmt);
    vsnprintf(errbuf, ERRMAX, fmt, ap);
    va_end(ap);

    v->err(errbuf, strlen(errbuf));
}

static void
verr(Verifier* v, struct Da64Inst* inst, const char* msg)
{
    char fmtbuf[128];
    da64_format(inst, fmtbuf);
    verrmin(v, "%x: %s: %s", v->addr, fmtbuf, msg);
}

static size_t
bundlesize(Verifier* v)
{
    switch (v->opts->bundle) {
    case LFI_BUNDLE8:
        return 8;
    case LFI_BUNDLE16:
        return 16;
    default:
        assert(!"invalid bundle size");
    }
}

enum {
    INSN_SIZE = 4,
};

#define INSN_NOP 0xd503201f

enum {
    REG_ADDR    = 18,
    REG_BASE    = 21,
    REG_GAS     = 23,
    REG_BUNDLE  = 24,
    REG_SYS     = 25,
    REG_RET     = 30,
};

enum {
    SYS_tpidr_el0        = 0xde82,
    SYS_fpsr             = 0xda21,
    SYS_fpcr             = 0xda20,
    SYS_id_aa64pfr0_el1  = 0xc020,
    SYS_id_aa64pfr1_el1  = 0xc021,
    SYS_id_aa64zfr0_el1  = 0xc024, // requires SVE
    SYS_id_aa64isar0_el1 = 0xc030,
    SYS_id_aa64isar1_el1 = 0xc031,
};

static bool
cfreg(Verifier* v, uint8_t reg)
{
    if (v->opts->bundle == LFI_BUNDLE_NONE) {
        return reg == REG_ADDR || reg == REG_RET;
    }
    return reg == REG_BUNDLE;
}

static bool
rtsysreg(Verifier* v, uint8_t reg)
{
    if (v->opts->poc)
        return reg == REG_SYS;
    return reg == REG_BASE;
}

static bool
basereg(uint8_t reg)
{
    return reg == REG_BASE;
}

static bool
retreg(uint8_t reg)
{
    return reg == REG_RET;
}

static bool
gasreg(uint8_t reg)
{
    return reg == REG_GAS;
}

static bool
fixedreg(Verifier* v, uint8_t reg)
{
    if (reg == REG_BASE)
        return true;
    if (v->opts->poc && reg == REG_SYS)
        return true;
    return false;
}

static bool
addrreg(Verifier* v, uint8_t reg, bool sp)
{
    if (sp && reg == 31)
        return true;
    if (cfreg(v, reg))
        return true;
    if (reg == REG_ADDR || reg == REG_RET)
        return true;
    return false;
}

static bool
sysreg(uint16_t sysreg)
{
    return sysreg == SYS_fpsr ||
        sysreg == SYS_fpcr ||
        sysreg == SYS_id_aa64pfr0_el1 ||
        sysreg == SYS_id_aa64pfr1_el1 ||
        sysreg == SYS_id_aa64zfr0_el1 ||
        sysreg == SYS_id_aa64isar0_el1 ||
        sysreg == SYS_id_aa64isar1_el1;
}

static int
nmod(struct Da64Inst* dinst)
{
    switch (DA64_GROUP(dinst->mnem)) {
    case DA64G_BRANCH:
    case DA64G_BCOND:
    case DA64G_BRANCHREG:
        return 0;
    case DA64G_LDATOMIC:
    case DA64G_SWP:
    case DA64G_CASP:
    case DA64G_CAS:
        return 2;
    }

    switch (dinst->mnem) {
    // load pair instructions
    case DA64I_LDPW_POST:
    case DA64I_LDPW:
    case DA64I_LDPW_PRE:
    case DA64I_LDPSW_POST:
    case DA64I_LDPSW:
    case DA64I_LDPSW_PRE:
    case DA64I_LDPX_POST:
    case DA64I_LDPX:
    case DA64I_LDPX_PRE:
    case DA64I_LDNPW:
    case DA64I_LDNPX:
    case DA64I_LDXPW:
    case DA64I_LDXPX:
    case DA64I_LDAXPW:
    case DA64I_LDAXPX:
        return 2;
    // stores
    case DA64I_STR_IMM:
    case DA64I_STR_REG:
    case DA64I_STRX_PRE:
    case DA64I_STRX_POST:
    case DA64I_STPX_POST:
    case DA64I_STPX:
    case DA64I_STPX_PRE:
        return 0;
    }

    return 1;
}

static void
chkbranch(Verifier* v, struct Da64Inst* dinst)
{
    switch (dinst->mnem) {
    case DA64I_BLR:
        if (rtsysreg(v, dinst->ops[0].reg))
            return;
        // fallthrough
    case DA64I_BR:
    case DA64I_RET:
        assert(dinst->ops[0].type == DA_OP_REGGP);
        if (!cfreg(v, dinst->ops[0].reg)) {
            verr(v, dinst, "indirect branch using illegal register");
        }
        break;
    default:
        assert(DA64_GROUP(dinst->mnem) != DA64G_BRANCHREG);
        break;
    }

    if (v->opts->bundle == LFI_BUNDLE_NONE)
        return;

    uint64_t target = 0;
    switch (dinst->mnem) {
    case DA64I_BCOND:
    case DA64I_B:
    case DA64I_BL:
        target = v->addr + dinst->imm64;
        break;
    case DA64I_CBZ:
    case DA64I_CBNZ:
        target = v->addr + dinst->imm64;
        break;
    case DA64I_TBZ:
    case DA64I_TBNZ:
        target = v->addr + dinst->imm64;
        break;
    default:
        return;
    }
    
    uint64_t bundle;
    switch (v->opts->bundle) {
    case LFI_BUNDLE8:
        bundle = 8;
        break;
    case LFI_BUNDLE16:
        bundle = 16;
        break;
    default:
        printf("%d\n", v->opts->bundle);
        assert(!"invalid bundle type");
        break;
    }
    if (target % bundle != 0) {
        verr(v, dinst, "direct branch target is not bundle-aligned");
    }
}

static void
chksys(Verifier* v, struct Da64Inst* dinst)
{
    switch (dinst->mnem) {
    case DA64I_MSR:
        assert(dinst->ops[0].type == DA_OP_SYSREG);
        if (!sysreg(dinst->ops[0].sysreg))
            verr(v, dinst, "write to illegal sysreg");
        break;
    case DA64I_MRS:
        assert(dinst->ops[1].type == DA_OP_SYSREG);
        if (!sysreg(dinst->ops[1].sysreg))
            verr(v, dinst, "read from illegal sysreg");
        break;
    }
}

static bool
okmnem(Verifier* v, struct Da64Inst* dinst)
{
    if (v->opts->decl) {
        switch (dinst->mnem) {
#include "decl.instrs"
        }
    } else {
        switch (dinst->mnem) {
#include "base.instrs"
        }
    }

    return false;
}

static bool
okmemop(Verifier* v, struct Da64Op* op)
{
    switch (op->type) {
    case DA_OP_MEMUOFF:
    case DA_OP_MEMSOFF:
        // runtime call
        if (rtsysreg(v, op->reg) && op->simm16 == 0)
            return true;
        return addrreg(v, op->reg, true);
    case DA_OP_MEMSOFFPRE:
    case DA_OP_MEMSOFFPOST:
        return addrreg(v, op->reg, true);
    case DA_OP_MEMREG:
        return basereg(op->reg) && op->memreg.ext == DA_EXT_UXTW &&
            op->memreg.sc == 0;
        break;
    case DA_OP_MEMREGPOST:
        return false;
    case DA_OP_MEMINC:
        return false;
    default:
        return true;
    }
}

static void
chkmemops(Verifier* v, struct Da64Inst* dinst)
{
    for (size_t i = 0; i < sizeof(dinst->ops) / sizeof(struct Da64Op); i++) {
        if (!okmemop(v, &dinst->ops[i]))
            verr(v, dinst, "illegal memory operand");
    }
}

static bool
okreadpoc(Verifier* v, struct Da64Inst* dinst, struct Da64Op* op)
{
    bool sf = false;
    bool sp = false;
    switch (op->type) {
    case DA_OP_REGSP:
        sp = true;
    case DA_OP_REGGP:
        sf = op->reggp.sf;
        break;
    case DA_OP_REGGPEXT:
        sf = op->reggpext.sf;
        break;
    default:
        return true;
    }

    if (dinst->mnem == DA64I_ADD_EXT) {
        // 'add addrreg, base, lo, uxtw' is allowed.
        if (addrreg(v, dinst->ops[0].reg, true) && basereg(dinst->ops[1].reg) &&
                dinst->ops[2].reggpext.ext == DA_EXT_UXTW &&
                dinst->ops[2].reggpext.sf == 0 && dinst->ops[2].reggpext.shift == 0)
            return true;
    }

    if (retreg(op->reg) && dinst->mnem == DA64I_LDR_IMM) {
        // 'ldr x30, [rtsysreg]' is allowed.
        if (dinst->ops[1].type == DA_OP_MEMUOFF && dinst->ops[1].uimm16 == 0 &&
                rtsysreg(v, dinst->ops[1].reg))
            return true;
    }

    if (v->opts->bundle != LFI_BUNDLE_NONE && dinst->mnem == DA64I_AND_IMM) {
        // 'bic x24, x18, 0x[f7]' is allowed
        if (cfreg(v, dinst->ops[0].reg) && addrreg(v, dinst->ops[1].reg, false) &&
                dinst->ops[2].type == DA_OP_IMMLARGE) {
            if (v->opts->bundle == LFI_BUNDLE8 && dinst->imm64 == 0xfffffffffffffff8)
                return true;
            else if (v->opts->bundle == LFI_BUNDLE16 && dinst->imm64 == 0xfffffffffffffff0)
                return true;
        }
    }

    if ((fixedreg(v, op->reg) || addrreg(v, op->reg, sp)) && sf)
        return false;
    return true;
}

static void
chkadr(Verifier* v, struct Da64Inst* dinst)
{
    if (!v->opts->poc)
        return;
    if (dinst->mnem == DA64I_ADR || dinst->mnem == DA64I_ADRP) {
        assert(dinst->ops[0].type == DA_OP_REGGP);
        if (!addrreg(v, dinst->ops[0].reg, false)) {
            verr(v, dinst, "adr/adrp must target address register");
        }
    }
}

static bool
okgasmod(Verifier* v, struct Da64Inst* dinst, struct Da64Op* op)
{
    if (op->type != DA_OP_REGGP &&
        op->type != DA_OP_REGGPINC &&
        op->type != DA_OP_REGGPEXT &&
        op->type != DA_OP_REGSP)
        return true;
    if (!gasreg(op->reg))
        return true;
    return false;
}

static bool
okmod(Verifier* v, struct Da64Inst* dinst, struct Da64Op* op)
{
    if (op->type != DA_OP_REGGP &&
        op->type != DA_OP_REGGPINC &&
        op->type != DA_OP_REGGPEXT &&
        op->type != DA_OP_REGSP)
        return true;

    if (fixedreg(v, op->reg))
        return false;
    if (!addrreg(v, op->reg, op->type == DA_OP_REGSP))
        return true;

    if (v->opts->poc && (dinst->mnem == DA64I_ADR || dinst->mnem == DA64I_ADRP))
        return true;

    if (dinst->mnem == DA64I_ADD_EXT) {
        // 'add addrreg, base, lo, uxtw' is allowed.
        if (addrreg(v, dinst->ops[0].reg, true) && basereg(dinst->ops[1].reg) &&
                dinst->ops[2].reggpext.ext == DA_EXT_UXTW &&
                dinst->ops[2].reggpext.sf == 0 && dinst->ops[2].reggpext.shift == 0)
            return true;
    }

    if (retreg(op->reg) && dinst->mnem == DA64I_LDR_IMM) {
        // 'ldr x30, [rtsysreg]' is allowed.
        if (dinst->ops[1].type == DA_OP_MEMUOFF && dinst->ops[1].uimm16 == 0 &&
                rtsysreg(v, dinst->ops[1].reg))
            return true;
    }

    if (v->opts->bundle != LFI_BUNDLE_NONE && dinst->mnem == DA64I_AND_IMM) {
        // 'bic x24, x18, 0x[f7]' is allowed
        if (cfreg(v, dinst->ops[0].reg) && addrreg(v, dinst->ops[1].reg, false) &&
                dinst->ops[2].type == DA_OP_IMMLARGE) {
            if (v->opts->bundle == LFI_BUNDLE8 && dinst->imm64 == 0xfffffffffffffff8)
                return true;
            else if (v->opts->bundle == LFI_BUNDLE16 && dinst->imm64 == 0xfffffffffffffff0)
                return true;
        }
    }

    return false;
}

static void
vchk(Verifier* v, uint32_t insn)
{
    struct Da64Inst dinst;
    da64_decode(insn, &dinst);

    if (dinst.mnem == DA64I_UNKNOWN) {
        verrmin(v, "%lx: unknown instruction: %x", v->addr, insn);
        return;
    }

    if (insn == INSN_NOP)
        return;

    if (!okmnem(v, &dinst))
        verr(v, &dinst, "illegal instruction");

    chkbranch(v, &dinst);
    chksys(v, &dinst);
    chkadr(v, &dinst);
    chkmemops(v, &dinst);

    int n = nmod(&dinst);
    for (int i = 0; i < n; i++) {
        if (!okmod(v, &dinst, &dinst.ops[i]))
            verr(v, &dinst, "illegal modification of reserved register");
    }
    if (v->opts->poc) {
        switch (dinst.mnem) {
        case DA64I_RET:
        case DA64I_BR:
        case DA64I_BLR:
            return;
        }
        for (int i = n; i < sizeof(dinst.ops) / sizeof(struct Da64Op); i++) {
            if (!okreadpoc(v, &dinst, &dinst.ops[i]))
                verr(v, &dinst, "illegal read of 64-bit address register");
        }
    }
}

static bool
branchinfo(Verifier* v, struct Da64Inst* dinst, int64_t* target, bool* indirect)
{
    *target = 0;
    *indirect = false;

    // Don't count runtime calls as branches.
    if (dinst->mnem == DA64I_BLR && rtsysreg(v, dinst->ops[0].reg))
        return false;

    bool branch = true;
    switch (dinst->mnem) {
    case DA64I_BLR:
    case DA64I_BR:
    case DA64I_RET:
        *indirect = true;
        break;
    case DA64I_BCOND:
    case DA64I_B:
    case DA64I_BL:
        *target = v->addr + dinst->imm64;
        break;
    case DA64I_CBZ:
    case DA64I_CBNZ:
        *target = v->addr + dinst->imm64;
        break;
    case DA64I_TBZ:
    case DA64I_TBNZ:
        *target = v->addr + dinst->imm64;
        break;
    default:
        branch = false;
        break;
    }
    return branch;
}

static uint32_t
ceilimm(uint32_t imm, uint32_t align)
{
    uint32_t mask = align - 1;
    return (imm + mask) & ~mask;
}

static uint32_t
subx23(int64_t imm)
{
    const uint32_t OP_SUB = 0b110100010UL << 23;
    if (imm < 4096) {
        return OP_SUB | (imm << 10) | (23 << 5) | (23);
    }
    if (imm % 4096 != 0) {
        imm = ceilimm(imm, 4096);
    }
    return OP_SUB | (1 << 22) | ((imm >> 12) << 10) | (23 << 5) | (23);
}

static void
vchkmeter(Verifier* v, uint32_t* insns, size_t n)
{
    bool* leaders = calloc(sizeof(bool) * n, 1);
    if (!leaders) {
        verrmin(v, "cannot allocate: out of memory");
        return;
    }

    bool* updates = calloc(sizeof(bool) * n, 1);
    if (!updates) {
        free(leaders);
        verrmin(v, "cannot allocate: out of memory");
        return;
    }

    struct Da64Inst* insts = calloc(sizeof(struct Da64Inst) * n, 1);
    if (!insts) {
        free(leaders);
        free(updates);
        verrmin(v, "cannot allocate: out of memory");
        return;
    }

    for (size_t i = 0; i < n; i++) {
        da64_decode(insns[i], &insts[i]);
        assert(insts[i].mnem != DA64I_UNKNOWN);
    }

    leaders[0] = true;

    uintptr_t addr = v->addr;

    // Calculate basic blocks by identifying leaders.
    for (size_t i = 0; i < n; i++) {
        struct Da64Inst dinst = insts[i];
        v->addr = addr + i * INSN_SIZE;

        int64_t target;
        bool indirect;
        if (!branchinfo(v, &dinst, &target, &indirect))
            continue;

        // Branch target is a leader.
        if (!indirect)
            leaders[target / INSN_SIZE] = true;
        // Instruction immediately following a branch is a leader.
        if (i + 1 < n)
            leaders[i + 1] = true;
    }

    size_t curleader = 0;
    for (size_t i = 0; i < n; i++) {
        struct Da64Inst dinst = insts[i];
        v->addr = addr + i * INSN_SIZE;

        if (leaders[i])
            curleader = i;

        int64_t target;
        bool indirect;
        bool branch = branchinfo(v, &dinst, &target, &indirect);
        if (!branch)
            continue;

        size_t ibundlesize = bundlesize(v) / INSN_SIZE;
        if (i % ibundlesize != ibundlesize - 1) {
            verr(v, &dinst, "branch must occur at the end of a bundle");
            continue;
        }

        int64_t bbsize = i - curleader + 1;
        if (i == 0) {
            verr(v, &dinst, "branch not preceded by metering sequence");
            continue;
        }
        if (insns[i - 1] != subx23(bbsize)) {
            verr(v, &dinst, "branch not preceded by correct metering sequence");
            printf("should be %ld %x %x\n", bbsize, subx23(bbsize), insns[i - 1]);
            continue;
        }
        updates[i - 1] = true;
    }

    for (size_t i = 0; i < n; i++) {
        struct Da64Inst dinst = insts[i];
        v->addr = addr + i * INSN_SIZE;

        if (updates[i])
            continue;
        int n = nmod(&dinst);
        for (int i = 0; i < n; i++) {
            if (!okgasmod(v, &dinst, &dinst.ops[i]))
                verr(v, &dinst, "illegal modification of gas register");
        }
    }

    free(leaders);
    free(updates);
    free(insts);
}

bool
lfiv_verify_arm64(void* code, size_t size, uintptr_t addr, LFIvOpts* opts)
{
    if (size % INSN_SIZE != 0)
        return false;

    uint32_t* insns = (uint32_t*) code;

    Verifier v = {
        .addr = addr,
        .err = opts->err,
        .opts = opts,
    };

    for (size_t i = 0; i < size / INSN_SIZE; i++) {
        vchk(&v, insns[i]);
        v.addr += INSN_SIZE;
        // Exit early if there is no error reporter.
        if (v.failed && v.err == NULL)
            return false;
    }

    if (opts->meter != LFI_METER_NONE) {
        v.addr = addr;
        vchkmeter(&v, insns, size / INSN_SIZE);
    }

    return !v.failed;
}
