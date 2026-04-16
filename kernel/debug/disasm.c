#include "debug/disasm.h"
#include "graphics/console.h"
#include "io/serial.h"
#include "types.h"

// ---------------------------------------------------------------------------
// String buffer
// ---------------------------------------------------------------------------
typedef struct { char d[96]; int n; } sb;

static void sb_s(sb *s, const char *t) { while (*t && s->n < 94) s->d[s->n++] = *t++; }
static void sb_c(sb *s, char c)        { if (s->n < 94) s->d[s->n++] = c; }
static void sb_hex(sb *s, ulong v) {
    const char *h = "0123456789abcdef";
    sb_s(s, "0x");
    if (v == 0) { sb_c(s, '0'); return; }
    char tmp[16]; int i = 0;
    while (v) { tmp[i++] = h[v & 0xf]; v >>= 4; }
    while (i--) sb_c(s, tmp[i]);
}
static void sb_disp(sb *s, int v) {
    if (v < 0) { sb_c(s, '-'); sb_hex(s, (ulong)(uint)-v); }
    else        { sb_c(s, '+'); sb_hex(s, (ulong)(uint) v); }
}

// ---------------------------------------------------------------------------
// Register names
// ---------------------------------------------------------------------------
static const char *rn64[16] = {
    "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
    "r8","r9","r10","r11","r12","r13","r14","r15"
};
static const char *rn32[16] = {
    "eax","ecx","edx","ebx","esp","ebp","esi","edi",
    "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"
};
static const char *rn16[16] = {
    "ax","cx","dx","bx","sp","bp","si","di",
    "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w"
};
static const char *rn8n[16] = {  // no REX: 4-7 = AH,CH,DH,BH
    "al","cl","dl","bl","ah","ch","dh","bh",
    "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"
};
static const char *rn8r[16] = {  // with REX: 4-7 = SPL,BPL,SIL,DIL
    "al","cl","dl","bl","spl","bpl","sil","dil",
    "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"
};

static const char *regname(int idx, int size, int has_rex) {
    idx &= 15;
    switch (size) {
    case 64: return rn64[idx];
    case 32: return rn32[idx];
    case 16: return rn16[idx];
    default: return has_rex ? rn8r[idx] : rn8n[idx];
    }
}

// ---------------------------------------------------------------------------
// Prefix decoder
// ---------------------------------------------------------------------------
typedef struct {
    int rex, rex_w, rex_r, rex_x, rex_b;
    int p66, p67;
    int len;
} pfx;

static pfx parse_pfx(const ubyte *p) {
    pfx f = {0};
    for (;;) {
        ubyte b = p[f.len];
        if (b == 0x66) { f.p66 = 1; f.len++; }
        else if (b == 0x67) { f.p67 = 1; f.len++; }
        else if (b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                 b == 0x26 || b == 0x2E || b == 0x36 ||
                 b == 0x3E || b == 0x64 || b == 0x65) { f.len++; }
        else if (b >= 0x40 && b <= 0x4F) {
            f.rex   = 1;
            f.rex_w = (b >> 3) & 1;
            f.rex_r = (b >> 2) & 1;
            f.rex_x = (b >> 1) & 1;
            f.rex_b =  b       & 1;
            f.len++;
        }
        else break;
    }
    return f;
}

static int nat_size(pfx *f) {
    if (f->rex_w) return 64;
    if (f->p66)   return 16;
    return 32;
}

// ---------------------------------------------------------------------------
// Immediate helpers
// ---------------------------------------------------------------------------
static int   rd_i8 (const ubyte *p) { return (signed char)*p; }
static int   rd_i16(const ubyte *p) { return (int)(short)((uint)p[0]|((uint)p[1]<<8)); }
static int   rd_i32(const ubyte *p) { return (int)((uint)p[0]|((uint)p[1]<<8)|((uint)p[2]<<16)|((uint)p[3]<<24)); }
static ulong rd_u64(const ubyte *p) {
    return (ulong)p[0]|((ulong)p[1]<<8)|((ulong)p[2]<<16)|((ulong)p[3]<<24)|
           ((ulong)p[4]<<32)|((ulong)p[5]<<40)|((ulong)p[6]<<48)|((ulong)p[7]<<56);
}

// ---------------------------------------------------------------------------
// ModRM / SIB / displacement formatter
// p = pointer to ModRM byte. Returns bytes consumed (ModRM + SIB + disp).
// ---------------------------------------------------------------------------
static int fmt_rm(sb *s, const ubyte *p, pfx *f, int size, ulong next_rip) {
    ubyte modrm = *p;
    int mod = (modrm >> 6) & 3;
    int rm  = (modrm & 7) | (f->rex_b << 3);
    int consumed = 1;

    if (mod == 3) {
        sb_s(s, regname(rm, size, f->rex));
        return 1;
    }

    if      (size ==  8) sb_s(s, "byte [");
    else if (size == 16) sb_s(s, "word [");
    else if (size == 32) sb_s(s, "dword [");
    else                 sb_s(s, "[");

    if ((rm & 7) == 4) {  // SIB
        ubyte sib  = p[consumed++];
        int scale  = 1 << ((sib >> 6) & 3);
        int idx    = ((sib >> 3) & 7) | (f->rex_x << 3);
        int base   = (sib & 7)        | (f->rex_b << 3);
        int nobase = (mod == 0 && (base & 7) == 5);

        if (!nobase) sb_s(s, rn64[base]);
        if ((idx & 7) != 4) {
            if (!nobase) sb_c(s, '+');
            sb_s(s, rn64[idx]);
            if (scale > 1) { sb_c(s, '*'); sb_c(s, '0' + scale); }
        }
        if (nobase || mod == 2) {
            int d = rd_i32(p + consumed); consumed += 4;
            if (d) sb_disp(s, d);
        } else if (mod == 1) {
            int d = rd_i8(p + consumed); consumed++;
            if (d) sb_disp(s, d);
        }
    } else if (mod == 0 && (rm & 7) == 5) {  // RIP-relative
        int d = rd_i32(p + consumed); consumed += 4;
        ulong abs_addr = next_rip + (ulong)(long)d;
        sb_s(s, "rip");
        if (d) sb_disp(s, d);
        sb_s(s, " {"); sb_hex(s, abs_addr); sb_c(s, '}');
    } else {
        sb_s(s, rn64[rm]);
        if (mod == 1) {
            int d = rd_i8(p + consumed); consumed++;
            if (d) sb_disp(s, d);
        } else if (mod == 2) {
            int d = rd_i32(p + consumed); consumed += 4;
            if (d) sb_disp(s, d);
        }
    }
    sb_c(s, ']');
    return consumed;
}

static const char *modrm_reg(ubyte modrm, pfx *f, int size) {
    return regname(((modrm >> 3) & 7) | (f->rex_r << 3), size, f->rex);
}
static int modrm_subop(ubyte modrm) { return (modrm >> 3) & 7; }

// ---------------------------------------------------------------------------
// GRP subop tables
// ---------------------------------------------------------------------------
static const char *grp1_nm[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
static const char *grp2_nm[] = {"rol","ror","rcl","rcr","shl","shr","???","sar"};
static const char *grp3_nm[] = {"test","test","not","neg","mul","imul","div","idiv"};
static const char *grp5_nm[] = {"inc","dec","call","callf","jmp","jmpf","push","???"};

// ---------------------------------------------------------------------------
// Instruction length decoder
// ---------------------------------------------------------------------------
#define _M   0x01
#define _I8  0x02
#define _I16 0x04
#define _IZ  0x06

static const ubyte op1f[256] = {
  _M,_M,_M,_M,_I8,_IZ,0,0, _M,_M,_M,_M,_I8,_IZ,0,0,    // 00-0F
  _M,_M,_M,_M,_I8,_IZ,0,0, _M,_M,_M,_M,_I8,_IZ,0,0,    // 10-1F
  _M,_M,_M,_M,_I8,_IZ,0,0, _M,_M,_M,_M,_I8,_IZ,0,0,    // 20-2F
  _M,_M,_M,_M,_I8,_IZ,0,0, _M,_M,_M,_M,_I8,_IZ,0,0,    // 30-3F
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                       // 40-4F REX
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                       // 50-5F PUSH/POP
  0,0,_M,_M, 0,0,0,0, _IZ,_M|_IZ,_I8,_M|_I8, 0,0,0,0,  // 60-6F
  _I8,_I8,_I8,_I8,_I8,_I8,_I8,_I8,                      // 70-77 Jcc
  _I8,_I8,_I8,_I8,_I8,_I8,_I8,_I8,                      // 78-7F
  _M|_I8,_M|_IZ,_M|_I8,_M|_I8, _M,_M,_M,_M,             // 80-87
  _M,_M,_M,_M,_M,_M,_M,_M,                               // 88-8F
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                       // 90-9F
  0,0,0,0, 0,0,0,0, _I8,_IZ, 0,0,0,0,0,0,                // A0-AF
  _I8,_I8,_I8,_I8,_I8,_I8,_I8,_I8,                      // B0-B7
  _IZ,_IZ,_IZ,_IZ,_IZ,_IZ,_IZ,_IZ,                      // B8-BF
  _M|_I8,_M|_I8,_I16,0, 0,0,_M|_I8,_M|_IZ,              // C0-C7
  0,0,_I16,0, _I8,_I8,0,0,                               // C8-CF
  _M,_M,_M,_M, _I8,_I8,0,0,                              // D0-D7
  _M,_M,_M,_M,_M,_M,_M,_M,                               // D8-DF x87
  _I8,_I8,_I8,_I8, _I8,_I8,_I8,_I8,                     // E0-E7
  _IZ,_IZ,0,_I8, 0,0,0,0,                                // E8-EF
  0,0,0,0,0,0,_M,_M, 0,0,0,0,0,0,_M,_M,                 // F0-FF
};
static const ubyte op2f[256] = {
  _M,_M,_M,_M,0,0,0,0, 0,0,0,0,0,0,_M,_M,               // 00-0F
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // 10-1F
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // 20-2F
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,                      // 30-3F
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // 40-4F CMOVcc
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // 50-5F
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // 60-6F
  _M|_I8,_M|_I8,_M|_I8,_M|_I8, _M,_M,_M,_M,             // 70-77
  _M,_M,_M,_M,_M,_M,_M,_M,                               // 78-7F
  _IZ,_IZ,_IZ,_IZ,_IZ,_IZ,_IZ,_IZ,                      // 80-87 Jcc rel32
  _IZ,_IZ,_IZ,_IZ,_IZ,_IZ,_IZ,_IZ,                      // 88-8F
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // 90-9F SETcc
  0,0,0,_M, _M|_I8,_M,0,0, 0,0,0,_M, _M|_I8,_M,_M,_M,  // A0-AF
  _M,_M,_M,_M,_M,_M,_M,_M,                               // B0-B7
  _M,0,_M|_I8,_M, _M,_M,_M,_M,                           // B8-BF
  _M,_M,_M|_I8,_M, _M|_I8,_M|_I8,_M|_I8,_M,             // C0-C7
  0,0,0,0,0,0,0,0,                                        // C8-CF BSWAP
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // D0-DF
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // E0-EF
  _M,_M,_M,_M,_M,_M,_M,_M, _M,_M,_M,_M,_M,_M,_M,_M,    // F0-FF
};

static int modrm_extra_len(const ubyte *mrm) {
    ubyte m = *mrm;
    int mod = (m >> 6) & 3, rm = m & 7, extra = 0;
    if (mod == 3) return 0;
    if (rm == 4) { extra++; ubyte sib = mrm[1]; if (mod == 0 && (sib & 7) == 5) extra += 4; }
    if      (mod == 0 && rm == 5) extra += 4;
    else if (mod == 1)            extra += 1;
    else if (mod == 2)            extra += 4;
    return extra;
}

static int x86_len(const ubyte *ip) {
    const ubyte *p = ip;
    int rex_w = 0, op66 = 0, op67 = 0;
    for (int i = 0; i < 15; i++) {
        ubyte b = *p;
        if (b == 0x66)   { op66 = 1; p++; }
        else if (b == 0x67)   { op67 = 1; p++; }
        else if (b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                 b == 0x26 || b == 0x2E || b == 0x36 ||
                 b == 0x3E || b == 0x64 || b == 0x65) { p++; }
        else if (b >= 0x40 && b <= 0x4F) { rex_w = (b >> 3) & 1; p++; }
        else break;
    }
    (void)op67;
    ubyte op = *p++; ubyte flags = 0; int imm = 0;
    if (op == 0x0F) {
        ubyte o2 = *p++;
        if (o2 == 0x38 || o2 == 0x3A) {
            p++; p += 1 + modrm_extra_len(p); if (o2 == 0x3A) p++;
            return (int)(p - ip);
        }
        flags = op2f[o2];
    } else if (op == 0xC4) { p += 2; p += 1 + modrm_extra_len(p); p++; return (int)(p-ip); }
    else if (op == 0xC5)   { p++;    p += 1 + modrm_extra_len(p);      return (int)(p-ip); }
    else if (op == 0xC8)   { return (int)(p - ip) + 3; }
    else if (op >= 0xA0 && op <= 0xA3) { return (int)(p - ip) + (op67 ? 4 : 8); }
    else if (op == 0xF6 || op == 0xF7) {
        ubyte m = *p++; p += modrm_extra_len(p-1);
        if (((m >> 3) & 7) <= 1) p += (op == 0xF6 ? 1 : (op66 ? 2 : 4));
        return (int)(p - ip);
    } else { flags = op1f[op]; }
    if (flags & _M) { p += 1 + modrm_extra_len(p); }
    switch (flags & 0x06) {
    case _I8:  imm = 1; break;
    case _I16: imm = 2; break;
    case _IZ:
        if (op == 0xE8 || op == 0xE9)       imm = 4;
        else if (rex_w) imm = (op >= 0xB8 && op <= 0xBF) ? 8 : 4;
        else            imm = op66 ? 2 : 4;
        break;
    }
    p += imm;
    int l = (int)(p - ip);
    return l < 1 ? 1 : l;
}

// ---------------------------------------------------------------------------
// Main instruction formatter — returns instruction length
// ---------------------------------------------------------------------------
static int disasm_insn(const ubyte *ip, ulong ip_addr, char *out, int out_sz) {
    sb s = {0};
    pfx f = parse_pfx(ip);
    const ubyte *p = ip + f.len;
    int len     = x86_len(ip);
    ulong nrip  = ip_addr + (ulong)len;   // RIP after this instruction
    int sz      = nat_size(&f);
    ubyte op    = *p++;

    // ---- 2-byte: 0F xx ----
    if (op == 0x0F) {
        ubyte o2 = *p++;
        if (o2 >= 0x80 && o2 <= 0x8F) {  // Jcc rel32
            static const char *jcc[] = {
                "jo","jno","jb","jae","je","jne","jbe","ja",
                "js","jns","jp","jnp","jl","jge","jle","jg"
            };
            sb_s(&s, jcc[o2 & 0xf]); sb_c(&s, ' ');
            sb_hex(&s, nrip + (ulong)(long)rd_i32(p));
        } else if (o2 >= 0x90 && o2 <= 0x9F) {  // SETcc
            static const char *setcc[] = {
                "seto","setno","setb","setae","sete","setne","setbe","seta",
                "sets","setns","setp","setnp","setl","setge","setle","setg"
            };
            sb_s(&s, setcc[o2 & 0xf]); sb_c(&s, ' ');
            fmt_rm(&s, p, &f, 8, nrip);
        } else if (o2 >= 0x40 && o2 <= 0x4F) {  // CMOVcc
            static const char *cmov[] = {
                "cmovo","cmovno","cmovb","cmovae","cmove","cmovne","cmovbe","cmova",
                "cmovs","cmovns","cmovp","cmovnp","cmovl","cmovge","cmovle","cmovg"
            };
            sb_s(&s, cmov[o2 & 0xf]); sb_c(&s, ' ');
            sb_s(&s, modrm_reg(*p, &f, sz)); sb_s(&s, ", ");
            fmt_rm(&s, p, &f, sz, nrip);
        } else switch (o2) {
        case 0x05: sb_s(&s, "syscall");  break;
        case 0x07: sb_s(&s, "sysret");   break;
        case 0x0B: sb_s(&s, "ud2");      break;
        case 0x31: sb_s(&s, "rdtsc");    break;
        case 0x34: sb_s(&s, "sysenter"); break;
        case 0x35: sb_s(&s, "sysexit");  break;
        case 0xAF:
            sb_s(&s, "imul "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, sz, nrip); break;
        case 0xB6:
            sb_s(&s, "movzx "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, 8, nrip); break;
        case 0xB7:
            sb_s(&s, "movzx "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, 16, nrip); break;
        case 0xBE:
            sb_s(&s, "movsx "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, 8, nrip); break;
        case 0xBF:
            sb_s(&s, "movsx "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, 16, nrip); break;
        case 0xA3:
            sb_s(&s, "bt "); fmt_rm(&s, p, &f, sz, nrip);
            sb_s(&s, ", "); sb_s(&s, modrm_reg(*p, &f, sz)); break;
        case 0xAB:
            sb_s(&s, "bts "); fmt_rm(&s, p, &f, sz, nrip);
            sb_s(&s, ", "); sb_s(&s, modrm_reg(*p, &f, sz)); break;
        case 0xBC:
            sb_s(&s, "bsf "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, sz, nrip); break;
        case 0xBD:
            sb_s(&s, "bsr "); sb_s(&s, modrm_reg(*p, &f, sz));
            sb_s(&s, ", "); fmt_rm(&s, p, &f, sz, nrip); break;
        default: sb_s(&s, "0f ??"); break;
        }
        goto done;
    }

    // ---- ADD/OR/ADC/SBB/AND/SUB/XOR/CMP (00-3D) ----
    if (op <= 0x3D && (op & 7) <= 5) {
        static const char *alu[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
        sb_s(&s, alu[(op >> 3) & 7]); sb_c(&s, ' ');
        switch (op & 7) {
        case 0: fmt_rm(&s,p,&f,8, nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,8));  break;
        case 1: fmt_rm(&s,p,&f,sz,nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,sz)); break;
        case 2: sb_s(&s,modrm_reg(*p,&f,8));  sb_s(&s,", "); fmt_rm(&s,p,&f,8, nrip); break;
        case 3: sb_s(&s,modrm_reg(*p,&f,sz)); sb_s(&s,", "); fmt_rm(&s,p,&f,sz,nrip); break;
        case 4: sb_s(&s,"al, ");            sb_hex(&s,(ulong)*p); break;
        case 5: sb_s(&s,regname(0,sz,0)); sb_s(&s,", ");
                sb_hex(&s, (ulong)(uint)rd_i32(p)); break;
        }
        goto done;
    }

    // ---- PUSH/POP reg (50-5F) ----
    if (op >= 0x50 && op <= 0x5F) {
        int reg = (op & 7) | (f.rex_b << 3);
        sb_s(&s, op < 0x58 ? "push " : "pop ");
        sb_s(&s, rn64[reg]);
        goto done;
    }

    // ---- MOVSXD (63) ----
    if (op == 0x63) {
        sb_s(&s, "movsxd "); sb_s(&s, modrm_reg(*p, &f, sz));
        sb_s(&s, ", "); fmt_rm(&s, p, &f, 32, nrip);
        goto done;
    }

    // ---- PUSH imm ----
    if (op == 0x68) { sb_s(&s, "push "); sb_hex(&s, (ulong)(uint)rd_i32(p)); goto done; }
    if (op == 0x6A) { sb_s(&s, "push "); sb_hex(&s, (ulong)(ubyte)rd_i8(p)); goto done; }

    // ---- IMUL r, r/m, imm ----
    if (op == 0x69) {
        sb_s(&s, "imul "); sb_s(&s, modrm_reg(*p, &f, sz)); sb_s(&s, ", ");
        int c = fmt_rm(&s, p, &f, sz, nrip);
        sb_s(&s, ", "); sb_hex(&s, (ulong)(uint)rd_i32(p + c));
        goto done;
    }
    if (op == 0x6B) {
        sb_s(&s, "imul "); sb_s(&s, modrm_reg(*p, &f, sz)); sb_s(&s, ", ");
        int c = fmt_rm(&s, p, &f, sz, nrip);
        sb_s(&s, ", "); sb_hex(&s, (ulong)(ubyte)rd_i8(p + c));
        goto done;
    }

    // ---- Jcc short (70-7F) ----
    if (op >= 0x70 && op <= 0x7F) {
        static const char *jcc[] = {
            "jo","jno","jb","jae","je","jne","jbe","ja",
            "js","jns","jp","jnp","jl","jge","jle","jg"
        };
        sb_s(&s, jcc[op & 0xf]); sb_c(&s, ' ');
        sb_hex(&s, nrip + (ulong)(long)rd_i8(p));
        goto done;
    }

    // ---- GRP1: 80 / 81 / 83 ----
    if (op == 0x80 || op == 0x81 || op == 0x83) {
        sb_s(&s, grp1_nm[modrm_subop(*p)]); sb_c(&s, ' ');
        int rsz = (op == 0x80) ? 8 : sz;
        int c = fmt_rm(&s, p, &f, rsz, nrip);
        sb_s(&s, ", ");
        if (op == 0x80 || op == 0x83) sb_hex(&s, (ulong)(ubyte)(ubyte)rd_i8(p + c));
        else if (f.rex_w)             sb_hex(&s, (ulong)(uint)rd_i32(p + c));
        else if (f.p66)               sb_hex(&s, (ulong)(ushort)(ushort)rd_i16(p + c));
        else                          sb_hex(&s, (ulong)(uint)rd_i32(p + c));
        goto done;
    }

    // ---- TEST / XCHG ----
    if (op == 0x84) { sb_s(&s,"test "); fmt_rm(&s,p,&f,8, nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,8));  goto done; }
    if (op == 0x85) { sb_s(&s,"test "); fmt_rm(&s,p,&f,sz,nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,sz)); goto done; }
    if (op == 0x86) { sb_s(&s,"xchg "); fmt_rm(&s,p,&f,8, nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,8));  goto done; }
    if (op == 0x87) { sb_s(&s,"xchg "); fmt_rm(&s,p,&f,sz,nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,sz)); goto done; }

    // ---- MOV 88-8B ----
    if (op == 0x88) { sb_s(&s,"mov "); fmt_rm(&s,p,&f,8, nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,8));  goto done; }
    if (op == 0x89) { sb_s(&s,"mov "); fmt_rm(&s,p,&f,sz,nrip); sb_s(&s,", "); sb_s(&s,modrm_reg(*p,&f,sz)); goto done; }
    if (op == 0x8A) { sb_s(&s,"mov "); sb_s(&s,modrm_reg(*p,&f,8));  sb_s(&s,", "); fmt_rm(&s,p,&f,8, nrip); goto done; }
    if (op == 0x8B) { sb_s(&s,"mov "); sb_s(&s,modrm_reg(*p,&f,sz)); sb_s(&s,", "); fmt_rm(&s,p,&f,sz,nrip); goto done; }

    // ---- LEA ----
    if (op == 0x8D) {
        // For LEA, suppress the size prefix on the memory side (it's an address, not typed data)
        sb_s(&s, "lea "); sb_s(&s, modrm_reg(*p, &f, sz)); sb_s(&s, ", ");
        ubyte m = *p; int mod=(m>>6)&3; int rm=(m&7)|(f.rex_b<<3); int c=1;
        sb_c(&s, '[');
        if ((rm&7)==4) {
            ubyte sib=p[c++]; int sc=1<<((sib>>6)&3);
            int idx=((sib>>3)&7)|(f.rex_x<<3), base=(sib&7)|(f.rex_b<<3);
            int nb=(mod==0&&(base&7)==5);
            if (!nb) sb_s(&s, rn64[base]);
            if ((idx&7)!=4) { if(!nb)sb_c(&s,'+'); sb_s(&s,rn64[idx]); if(sc>1){sb_c(&s,'*');sb_c(&s,'0'+sc);} }
            if (nb||mod==2) { int d=rd_i32(p+c); c+=4; if(d) sb_disp(&s,d); }
            else if (mod==1){ int d=rd_i8(p+c);  c++;   if(d) sb_disp(&s,d); }
        } else if (mod==0&&(rm&7)==5) {
            int d=rd_i32(p+c); c+=4;
            ulong abs=nrip+(ulong)(long)d;
            sb_s(&s,"rip"); if(d)sb_disp(&s,d); sb_s(&s," {"); sb_hex(&s,abs); sb_c(&s,'}');
        } else {
            sb_s(&s, rn64[rm]);
            if (mod==1){int d=rd_i8(p+c); c++; if(d)sb_disp(&s,d);}
            else if(mod==2){int d=rd_i32(p+c); c+=4; if(d)sb_disp(&s,d);}
        }
        sb_c(&s, ']');
        goto done;
    }

    // ---- POP r/m (8F) ----
    if (op == 0x8F) { sb_s(&s,"pop "); fmt_rm(&s,p,&f,sz,nrip); goto done; }

    // ---- NOP ----
    if (op == 0x90) { sb_s(&s, "nop"); goto done; }

    // ---- XCHG rAX, r (91-97) ----
    if (op >= 0x91 && op <= 0x97) {
        int reg = (op & 7) | (f.rex_b << 3);
        sb_s(&s, "xchg "); sb_s(&s, regname(0, sz, 0));
        sb_s(&s, ", "); sb_s(&s, regname(reg, sz, f.rex));
        goto done;
    }

    // ---- CBW/CWDE/CDQE / CWD/CDQ/CQO ----
    if (op == 0x98) { sb_s(&s, f.rex_w ? "cdqe" : (f.p66 ? "cbw" : "cwde")); goto done; }
    if (op == 0x99) { sb_s(&s, f.rex_w ? "cqo"  : (f.p66 ? "cwd" : "cdq"));  goto done; }

    // ---- TEST rAX, imm ----
    if (op == 0xA8) { sb_s(&s,"test al, "); sb_hex(&s, (ulong)*p); goto done; }
    if (op == 0xA9) {
        sb_s(&s,"test "); sb_s(&s, regname(0, sz, 0)); sb_s(&s, ", ");
        sb_hex(&s, f.rex_w ? (ulong)(uint)rd_i32(p) : (ulong)(uint)rd_i32(p));
        goto done;
    }

    // ---- MOV reg8, imm8 (B0-B7) ----
    if (op >= 0xB0 && op <= 0xB7) {
        int reg = (op & 7) | (f.rex_b << 3);
        sb_s(&s,"mov "); sb_s(&s, regname(reg, 8, f.rex));
        sb_s(&s,", "); sb_hex(&s, (ulong)*p);
        goto done;
    }
    // ---- MOV reg, imm (B8-BF) ----
    if (op >= 0xB8 && op <= 0xBF) {
        int reg = (op & 7) | (f.rex_b << 3);
        sb_s(&s,"mov "); sb_s(&s, regname(reg, sz, f.rex)); sb_s(&s,", ");
        if (f.rex_w) sb_hex(&s, rd_u64(p));
        else if (f.p66) sb_hex(&s, (ulong)(ushort)rd_i16(p));
        else         sb_hex(&s, (ulong)(uint)rd_i32(p));
        goto done;
    }

    // ---- GRP2: C0/C1/D0/D1/D2/D3 ----
    if (op == 0xC0 || op == 0xD0 || op == 0xD2) {
        sb_s(&s, grp2_nm[modrm_subop(*p)]); sb_c(&s,' ');
        int c = fmt_rm(&s, p, &f, 8, nrip);
        if (op==0xC0) { sb_s(&s,", "); sb_hex(&s, (ulong)p[c]); }
        else if (op==0xD0) sb_s(&s, ", 1");
        else               sb_s(&s, ", cl");
        goto done;
    }
    if (op == 0xC1 || op == 0xD1 || op == 0xD3) {
        sb_s(&s, grp2_nm[modrm_subop(*p)]); sb_c(&s,' ');
        int c = fmt_rm(&s, p, &f, sz, nrip);
        if (op==0xC1) { sb_s(&s,", "); sb_hex(&s, (ulong)p[c]); }
        else if (op==0xD1) sb_s(&s, ", 1");
        else               sb_s(&s, ", cl");
        goto done;
    }

    // ---- RET ----
    if (op == 0xC3) { sb_s(&s, "ret"); goto done; }
    if (op == 0xC2) { sb_s(&s,"ret "); sb_hex(&s, (ulong)(ushort)rd_i16(p)); goto done; }

    // ---- LEAVE ----
    if (op == 0xC9) { sb_s(&s, "leave"); goto done; }

    // ---- MOV r/m, imm: C6 / C7 ----
    if (op == 0xC6) {
        sb_s(&s,"mov "); int c = fmt_rm(&s, p, &f, 8, nrip);
        sb_s(&s,", "); sb_hex(&s, (ulong)p[c]);
        goto done;
    }
    if (op == 0xC7) {
        sb_s(&s,"mov "); int c = fmt_rm(&s, p, &f, sz, nrip);
        sb_s(&s,", ");
        if (f.rex_w) sb_hex(&s, (ulong)(uint)rd_i32(p+c));
        else if (f.p66) sb_hex(&s, (ulong)(ushort)rd_i16(p+c));
        else         sb_hex(&s, (ulong)(uint)rd_i32(p+c));
        goto done;
    }

    // ---- INT ----
    if (op == 0xCC) { sb_s(&s, "int3"); goto done; }
    if (op == 0xCD) { sb_s(&s,"int "); sb_hex(&s, (ulong)*p); goto done; }

    // ---- CALL / JMP ----
    if (op == 0xE8) { sb_s(&s,"call "); sb_hex(&s, nrip+(ulong)(long)rd_i32(p)); goto done; }
    if (op == 0xE9) { sb_s(&s,"jmp ");  sb_hex(&s, nrip+(ulong)(long)rd_i32(p)); goto done; }
    if (op == 0xEB) { sb_s(&s,"jmp ");  sb_hex(&s, nrip+(ulong)(long)rd_i8(p));  goto done; }

    // ---- Single-byte misc ----
    if (op == 0xF4) { sb_s(&s, "hlt");  goto done; }
    if (op == 0xFA) { sb_s(&s, "cli");  goto done; }
    if (op == 0xFB) { sb_s(&s, "sti");  goto done; }
    if (op == 0xFC) { sb_s(&s, "cld");  goto done; }
    if (op == 0xFD) { sb_s(&s, "std");  goto done; }
    if (op == 0xF8) { sb_s(&s, "clc");  goto done; }
    if (op == 0xF9) { sb_s(&s, "stc");  goto done; }

    // ---- GRP3: F6 / F7 ----
    if (op == 0xF6 || op == 0xF7) {
        int rsz = (op == 0xF6) ? 8 : sz;
        int sub = modrm_subop(*p);
        sb_s(&s, grp3_nm[sub]); sb_c(&s, ' ');
        int c = fmt_rm(&s, p, &f, rsz, nrip);
        if (sub <= 1) {
            sb_s(&s, ", ");
            sb_hex(&s, op==0xF6 ? (ulong)p[c] : (ulong)(uint)rd_i32(p+c));
        }
        goto done;
    }

    // ---- GRP4 / GRP5: FE / FF ----
    if (op == 0xFE) {
        sb_s(&s, modrm_subop(*p) == 0 ? "inc " : "dec ");
        fmt_rm(&s, p, &f, 8, nrip);
        goto done;
    }
    if (op == 0xFF) {
        sb_s(&s, grp5_nm[modrm_subop(*p)]); sb_c(&s, ' ');
        fmt_rm(&s, p, &f, sz, nrip);
        goto done;
    }

    // Fallback
    { const char *h="0123456789abcdef"; sb_s(&s,"db 0x"); sb_c(&s,h[op>>4]); sb_c(&s,h[op&0xf]); }

done:
    s.d[s.n] = 0;
    int i; for (i = 0; i < out_sz-1 && i < s.n; i++) out[i] = s.d[i];
    out[i] = 0;
    return len;
}

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------
static void cprint(const char *s) { console_write(s); serial_write(s); }

static void print_hex64(ulong v) {
    const char *h = "0123456789abcdef";
    char buf[17]; buf[16] = 0;
    for (int i = 15; i >= 0; i--) { buf[i] = h[v & 0xf]; v >>= 4; }
    cprint(buf);
}
static void print_hex8(ubyte b) {
    const char *h = "0123456789abcdef";
    char buf[3] = { h[b>>4], h[b&0xf], 0 };
    cprint(buf);
}

// ---------------------------------------------------------------------------
// Backward scan: find instruction addresses leading up to `rip`
// ---------------------------------------------------------------------------
static int collect_before(ulong start, ulong rip, ulong *addrs, int max) {
    const ubyte *p = (const ubyte *)start;
    const ubyte *end = (const ubyte *)rip;
    int count = 0;
    while (p < end) {
        if (count < max) addrs[count++] = (ulong)p;
        else { for (int i = 0; i < max-1; i++) addrs[i] = addrs[i+1]; addrs[max-1] = (ulong)p; }
        p += x86_len(p);
    }
    return ((ulong)p == rip) ? count : -1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void disasm_around(ulong rip, int n) {
    cprint("\n--- disasm ---\n");

    ulong before_addrs[16];
    int before_count = 0;
    int want = (n < 15) ? n : 15;

    for (int back = 1; back <= 15 * (n + 1); back++) {
        int c = collect_before(rip - back, rip, before_addrs, want);
        if (c > before_count) {
            before_count = c;
            if (c >= want) break;
        }
    }

    char buf[96];

    for (int i = 0; i < before_count; i++) {
        ulong addr = before_addrs[i];
        const ubyte *bp = (const ubyte *)addr;
        int ilen = disasm_insn(bp, addr, buf, sizeof(buf));
        cprint("   "); print_hex64(addr); cprint(":  ");
        for (int j = 0; j < ilen && j < 8; j++) { print_hex8(bp[j]); cprint(" "); }
        for (int j = ilen; j < 8; j++) cprint("   ");
        cprint(" "); cprint(buf); cprint("\n");
    }

    {
        const ubyte *bp = (const ubyte *)rip;
        int ilen = disasm_insn(bp, rip, buf, sizeof(buf));
        cprint(">> "); print_hex64(rip); cprint(":  ");
        for (int j = 0; j < ilen && j < 8; j++) { print_hex8(bp[j]); cprint(" "); }
        for (int j = ilen; j < 8; j++) cprint("   ");
        cprint(" "); cprint(buf); cprint("   <-- RIP\n");
    }

    const ubyte *p = (const ubyte *)rip + x86_len((const ubyte *)rip);
    for (int i = 0; i < n; i++) {
        ulong addr = (ulong)p;
        int ilen = disasm_insn(p, addr, buf, sizeof(buf));
        cprint("   "); print_hex64(addr); cprint(":  ");
        for (int j = 0; j < ilen && j < 8; j++) { print_hex8(p[j]); cprint(" "); }
        for (int j = ilen; j < 8; j++) cprint("   ");
        cprint(" "); cprint(buf); cprint("\n");
        p += ilen;
    }
    cprint("--------------\n");
}
