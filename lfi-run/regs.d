module regs;

import proc;
import main = main;

struct Regs {
    ulong x0;
    ulong x1;
    ulong x2;
    ulong x3;
    ulong x4;
    ulong x5;
    ulong x6;
    ulong x7;
    ulong x8;
    ulong x9;
    ulong x10;
    ulong x11;
    ulong x12;
    ulong x13;
    ulong x14;
    ulong x15;
    ulong x16;
    ulong x17;
    ulong x18;
    ulong x19;
    ulong x20;
    ulong x21;
    ulong x22;
    ulong x23;
    ulong x24;
    ulong x25;
    ulong x26;
    ulong x27;
    ulong x28;
    ulong x29;
    ulong x30;
    ulong sp;
    ulong nzcv;
    ulong fpsr;
    ulong tpidr;
    ulong[64] vector;

    this(ulong base, ulong entry) {
        x21 = base;
        x30 = entry;
        x18 = base;
        if (main.flags.gas) {
            // max gas
            x23 = 0x0fffffffffffffff;
            // x23 = 100_000_000_000;
        } else {
            x23 = base;
            x24 = base;
        }
    }

    void validate(Proc* p) {
        x21 = p.base;
        x30 = p.addr(x30);
        x18 = p.addr(x18);
        if (!main.flags.gas) {
            x23 = p.addr(x23);
            x24 = p.addr(x24);
        }
        sp = p.addr(sp);
    }
};

struct Context {
    ulong x30;
    ulong sp;
    ulong x19;
    ulong x20;
    ulong x21;
    ulong x22;
    ulong x23;
    ulong x24;
    ulong x25;
    ulong x26;
    ulong x27;
    ulong x28;
    ulong x29;
    ulong d8;
    ulong d9;
    ulong d10;
    ulong d11;
    ulong d12;
    ulong d13;
    ulong d14;
    ulong d15;
    void* sp_base;

    this(uintptr sp, uintptr entry, void* sp_base) {
        this.sp = sp;
        this.sp_base = sp_base;
        this.x30 = entry;
    }
};
