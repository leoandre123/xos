// Minimal ACPI AML scanner + evaluator.
//
// Two-pass approach:
//   Pass 1 (scan):  Walk the AML byte stream to build tables of
//                   EC field definitions and battery method locations.
//   Pass 2 (eval):  Execute _BST / _BIF method bodies with a tiny
//                   stack machine that handles the most common patterns.
//
// Known limitations:
//   - Does not implement a full AML namespace or method call chain.
//   - Arithmetic ops (Add, Shift, etc.) evaluate both operands but
//     return 0 for the result — sufficient for detecting the return
//     Package even when the body has intermediate computations.
//   - Field accesses outside EmbeddedControl (e.g. system-memory
//     mapped registers) are not supported and return 0.

#include "aml.h"
#include "acpi/ec.h"
#include "io/logging.h"
#include "memory/memutils.h"
#include <stddef.h>

// ---------------------------------------------------------------------------
// AML opcode constants
// ---------------------------------------------------------------------------
#define AML_ZERO      0x00
#define AML_ONE       0x01
#define AML_ONES      0xFF
#define AML_ALIAS     0x06
#define AML_NAME      0x08
#define AML_BYTE      0x0A
#define AML_WORD      0x0B
#define AML_DWORD     0x0C
#define AML_STRING    0x0D
#define AML_QWORD     0x0E
#define AML_SCOPE     0x10
#define AML_BUFFER    0x11
#define AML_PACKAGE   0x12
#define AML_VARPACKAGE 0x13
#define AML_METHOD    0x14
#define AML_DUAL_NAME 0x2E
#define AML_MULTI_NAME 0x2F
#define AML_ROOT_CHAR 0x5C
#define AML_PARENT_CHAR 0x5E
#define AML_EXT_PREFIX 0x5B
// Extended opcodes (after 0x5B):
#define AML_EXT_MUTEX    0x01
#define AML_EXT_EVENT    0x02
#define AML_EXT_OPREGION 0x80
#define AML_EXT_FIELD    0x81
#define AML_EXT_DEVICE   0x82
#define AML_EXT_PROCESSOR 0x83
#define AML_EXT_POWER    0x84
#define AML_EXT_THERMAL  0x85
#define AML_EXT_IDXFIELD 0x86
#define AML_EXT_BANKFIELD 0x87
#define AML_EXT_CONDREF  0x12
// Statement / expression opcodes:
#define AML_LOCAL0    0x60
#define AML_LOCAL7    0x67
#define AML_ARG0      0x68
#define AML_ARG5      0x6D
#define AML_STORE     0x70
#define AML_ADD       0x72
#define AML_SUBTRACT  0x74
#define AML_MULTIPLY  0x77
#define AML_DIVIDE    0x78
#define AML_SHIFTL    0x79
#define AML_SHIFTR    0x7A
#define AML_AND       0x7B
#define AML_NAND      0x7C
#define AML_OR        0x7D
#define AML_NOR       0x7E
#define AML_XOR       0x7F
#define AML_NOT       0x80
#define AML_INC       0x75
#define AML_DEC       0x76
#define AML_CONCAT    0x73
#define AML_LNOT      0x92
#define AML_LAND      0x90
#define AML_LOR       0x91
#define AML_LEQ       0x93
#define AML_LGT       0x94
#define AML_LLT       0x95
#define AML_TOINT     0x99
#define AML_INDEX     0x88
#define AML_DEREFOF   0x83
#define AML_SIZEOF    0x87
#define AML_IF        0xA0
#define AML_ELSE      0xA1
#define AML_WHILE     0xA2
#define AML_NOOP      0xA3
#define AML_RETURN    0xA4
#define AML_BREAK     0xA5

// ACPI address spaces
#define ADDRSPACE_EC 0x03

// ---------------------------------------------------------------------------
// EC field table (built during scan phase)
// ---------------------------------------------------------------------------
#define MAX_EC_FIELDS 128

typedef struct {
  char name[5];    // 4-char field name + NUL
  uint bit_offset; // bit offset within EC address space
  uint bit_width;  // field width in bits
} ec_field_t;

static ec_field_t g_ec_fields[MAX_EC_FIELDS];
static uint g_ec_field_count;

static ec_field_t *find_ec_field(const char *name) {
  for (uint i = 0; i < g_ec_field_count; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++)
      if (g_ec_fields[i].name[j] != name[j]) { match = false; break; }
    if (match) return &g_ec_fields[i];
  }
  return NULL;
}

static ulong read_ec_field(const ec_field_t *f) {
  uint byte_off = f->bit_offset / 8;
  uint bytes    = (f->bit_width + 7) / 8;
  if (bytes > 8) bytes = 8;
  ulong val = 0;
  for (uint i = 0; i < bytes; i++)
    val |= (ulong)ec_read(byte_off + i) << (8 * i);
  if (f->bit_width < 64)
    val &= ((ulong)1 << f->bit_width) - 1;
  return val;
}

// ---------------------------------------------------------------------------
// Battery device table (built during scan phase)
// ---------------------------------------------------------------------------
#define MAX_BATTERIES 4

typedef struct {
  bool      present;
  const ubyte *bst_aml; // pointer into original AML buffer
  uint      bst_len;
  const ubyte *bif_aml;
  uint      bif_len;
} battery_dev_t;

static battery_dev_t g_batteries[MAX_BATTERIES];
static uint g_battery_count;

uint aml_battery_count(void) { return g_battery_count; }

// ---------------------------------------------------------------------------
// Stream helper (scan phase)
// ---------------------------------------------------------------------------
typedef struct {
  const ubyte *data;
  uint pos;
  uint end; // exclusive upper bound for this scope
} scan_ctx;

static ubyte sc_peek(const scan_ctx *s) {
  return s->pos < s->end ? s->data[s->pos] : 0;
}

static ubyte sc_read(scan_ctx *s) {
  return s->pos < s->end ? s->data[s->pos++] : 0;
}

// Read a PkgLength.  Returns the *absolute* end position of the pkglen block
// (i.e. start_of_pkglen_byte + encoded_total_length).
// Advances s->pos past the PkgLength bytes.
static uint sc_pkglen(scan_ctx *s) {
  if (s->pos >= s->end) return s->pos;
  uint start = s->pos;
  ubyte b0 = sc_read(s);
  uint n = (b0 >> 6) & 3;
  if (n == 0) return start + (b0 & 0x3F);
  uint len = b0 & 0x0F;
  for (uint i = 0; i < n && s->pos < s->end; i++)
    len |= (uint)sc_read(s) << (4 + 8 * i);
  return start + len;
}

// Read a 4-byte NameSeg into out[4] (no NUL added).
static void sc_nameseg(scan_ctx *s, char out[4]) {
  for (int i = 0; i < 4; i++)
    out[i] = (s->pos < s->end) ? (char)sc_read(s) : '_';
}

// Read a NameString, storing the *last* NameSeg into seg[5] (NUL-terminated).
static void sc_namestring(scan_ctx *s, char seg[5]) {
  memset8((ubyte *)seg, 0, 5);
  while (s->pos < s->end &&
         (sc_peek(s) == AML_ROOT_CHAR || sc_peek(s) == AML_PARENT_CHAR))
    sc_read(s);

  if (s->pos >= s->end) return;
  ubyte b = sc_peek(s);

  if (b == 0x00) { sc_read(s); return; } // NullName
  if (b == AML_DUAL_NAME) {
    sc_read(s);
    char tmp[4];
    sc_nameseg(s, tmp); // first seg (ignored)
    sc_nameseg(s, seg); // second seg (we keep)
    seg[4] = 0;
    return;
  }
  if (b == AML_MULTI_NAME) {
    sc_read(s);
    ubyte count = sc_read(s);
    char tmp[4];
    for (ubyte i = 0; i < count && s->pos + 4 <= s->end; i++)
      sc_nameseg(s, i + 1 == count ? seg : tmp);
    seg[4] = 0;
    return;
  }
  // Plain NameSeg (lead char is A-Z or '_')
  if ((b >= 'A' && b <= 'Z') || b == '_') {
    sc_nameseg(s, seg);
    seg[4] = 0;
  }
}

// Skip a TermArg that we don't care about evaluating.
// For compound terms we read the PkgLength to jump past them.
static void sc_skip_term(scan_ctx *s);

static void sc_skip_termarg(scan_ctx *s) { sc_skip_term(s); }

static void sc_skip_term(scan_ctx *s) {
  if (s->pos >= s->end) return;
  ubyte op = sc_read(s);
  switch (op) {
  // Simple fixed-size objects
  case AML_ZERO: case AML_ONE: case AML_ONES: case AML_NOOP:
  case AML_BREAK: break;
  case AML_BYTE:   s->pos += 1; break;
  case AML_WORD:   s->pos += 2; break;
  case AML_DWORD:  s->pos += 4; break;
  case AML_QWORD:  s->pos += 8; break;
  case AML_STRING:
    while (s->pos < s->end && s->data[s->pos]) s->pos++;
    if (s->pos < s->end) s->pos++;
    break;
  // Local/Arg references
  case AML_LOCAL0: case 0x61: case 0x62: case 0x63:
  case 0x64: case 0x65: case 0x66: case AML_LOCAL7:
  case AML_ARG0: case 0x69: case 0x6A: case 0x6B:
  case 0x6C: case AML_ARG5: break;
  // Compound: read PkgLength and jump to end
  case AML_SCOPE: case AML_BUFFER: case AML_PACKAGE:
  case AML_VARPACKAGE: case AML_METHOD:
  case AML_IF: case AML_ELSE: case AML_WHILE: {
    uint end = sc_pkglen(s);
    if (end < s->end) s->pos = end; else s->pos = s->end;
    break;
  }
  // NameOp: NameString DataRefObject
  case AML_NAME: {
    char seg[5]; sc_namestring(s, seg);
    sc_skip_term(s);
    break;
  }
  // Store: TermArg SuperName  (SuperName is also a TermArg in practice)
  case AML_STORE: sc_skip_termarg(s); sc_skip_termarg(s); break;
  // Unary ops
  case AML_INC: case AML_DEC: case AML_NOT: case AML_LNOT:
  case AML_DEREFOF: case AML_SIZEOF:
    sc_skip_termarg(s); break;
  // Binary ops: TermArg TermArg Target
  case AML_ADD: case AML_SUBTRACT: case AML_MULTIPLY:
  case AML_SHIFTL: case AML_SHIFTR: case AML_AND:
  case AML_NAND: case AML_OR: case AML_NOR: case AML_XOR:
  case AML_CONCAT: case AML_LEQ: case AML_LGT: case AML_LLT:
  case AML_LAND: case AML_LOR: case AML_INDEX:
    sc_skip_termarg(s); sc_skip_termarg(s); sc_skip_termarg(s); break;
  // Divide: 4 args
  case AML_DIVIDE:
    sc_skip_termarg(s); sc_skip_termarg(s);
    sc_skip_termarg(s); sc_skip_termarg(s); break;
  // Return / ToInt: 1 TermArg
  case AML_RETURN: case AML_TOINT: sc_skip_termarg(s); break;
  // Extended prefix
  case AML_EXT_PREFIX: {
    if (s->pos >= s->end) break;
    ubyte extop = sc_read(s);
    switch (extop) {
    case AML_EXT_MUTEX:
      { char seg[5]; sc_namestring(s, seg); s->pos++; break; } // +SyncLevel
    case AML_EXT_EVENT:
      { char seg[5]; sc_namestring(s, seg); break; }
    case AML_EXT_OPREGION:
      // NameString RegionSpace RegionOffset RegionLen  (no pkglen)
      { char seg[5]; sc_namestring(s, seg);
        s->pos++; // RegionSpace byte
        sc_skip_termarg(s); sc_skip_termarg(s); break; }
    case AML_EXT_FIELD: case AML_EXT_DEVICE: case AML_EXT_PROCESSOR:
    case AML_EXT_POWER: case AML_EXT_THERMAL: case AML_EXT_IDXFIELD:
    case AML_EXT_BANKFIELD: {
      uint end = sc_pkglen(s);
      if (end < s->end) s->pos = end; else s->pos = s->end;
      break;
    }
    default: break; // unknown ext — advance by 0 (safe)
    }
    break;
  }
  // NameString reference (starts with A-Z or '_')
  case AML_ROOT_CHAR: case AML_PARENT_CHAR: {
    s->pos--; // put back root/parent char
    char seg[5]; sc_namestring(s, seg); break;
  }
  default:
    if ((op >= 'A' && op <= 'Z') || op == '_') {
      // NameSeg — 3 more bytes to complete the 4-char segment
      s->pos += 3;
    }
    // Otherwise truly unknown; we've already consumed 1 byte — safest to stop.
    break;
  }
}

// ---------------------------------------------------------------------------
// Field list parser (inside a FieldOp body)
// ---------------------------------------------------------------------------
static void sc_parse_field_list(scan_ctx *s, uint end, uint ec_bit_base) {
  while (s->pos < end) {
    ubyte b = sc_peek(s);
    if (b == 0x00) { // ReservedField: 00 PkgLength
      sc_read(s);
      uint bits = 0;
      ubyte pb = sc_read(s);
      uint n = (pb >> 6) & 3;
      if (n == 0) bits = pb & 0x3F;
      else {
        bits = pb & 0x0F;
        for (uint i = 0; i < n && s->pos < end; i++)
          bits |= (uint)sc_read(s) << (4 + 8*i);
      }
      ec_bit_base += bits;
    } else if (b == 0x01) { // AccessField: 01 AccessType AccessAttrib
      sc_read(s); sc_read(s); sc_read(s);
    } else if (b == 0x02) { // ConnectField: 02 ...  (ACPI 5+, skip)
      sc_read(s); sc_skip_term(s);
    } else if ((b >= 'A' && b <= 'Z') || b == '_') {
      // NamedField: NameSeg PkgLength(bit-width)
      char seg[5];
      sc_nameseg(s, seg); seg[4] = 0;
      // PkgLength encodes the field bit-width
      uint pkgstart = s->pos;
      uint bits = 0;
      ubyte pb = sc_read(s);
      uint n = (pb >> 6) & 3;
      if (n == 0) bits = pb & 0x3F;
      else {
        bits = pb & 0x0F;
        for (uint i = 0; i < n && s->pos < end; i++)
          bits |= (uint)sc_read(s) << (4 + 8*i);
      }
      (void)pkgstart;
      if (g_ec_field_count < MAX_EC_FIELDS) {
        ec_field_t *f = &g_ec_fields[g_ec_field_count++];
        memcpy8((ubyte *)f->name, (ubyte *)seg, 5);
        f->bit_offset = ec_bit_base;
        f->bit_width  = bits;
      }
      ec_bit_base += bits;
    } else {
      break; // unrecognised — stop
    }
  }
}

// ---------------------------------------------------------------------------
// Forward declaration for recursive scan
// ---------------------------------------------------------------------------
static void sc_scan_termlist(scan_ctx *s);

// ---------------------------------------------------------------------------
// Scan one term at s->pos.  The caller's loop advances through the termlist.
// ---------------------------------------------------------------------------
static void sc_scan_term(scan_ctx *s) {
  if (s->pos >= s->end) return;
  ubyte op = sc_peek(s);

  // ----- ScopeOp / DeviceOp -----
  if (op == AML_SCOPE || op == AML_EXT_PREFIX) {
    bool is_device = false;
    if (op == AML_EXT_PREFIX) {
      s->pos++;
      if (s->pos >= s->end) return;
      ubyte extop = sc_peek(s);
      if (extop != AML_EXT_DEVICE) {
        s->pos--; // not a Device — let sc_skip_term handle it
        sc_skip_term(s);
        return;
      }
      sc_read(s); // consume extop
      is_device = true;
    } else {
      sc_read(s); // consume ScopeOp
    }

    uint body_end = sc_pkglen(s);
    if (body_end > s->end) body_end = s->end;
    char seg[5];
    sc_namestring(s, seg);

    // Recurse into the scope/device body
    scan_ctx inner = { s->data, s->pos, body_end };
    sc_scan_termlist(&inner);
    s->pos = body_end;

    if (is_device) {
      // Check if any newly-added field from this scan was a battery marker.
      // (Battery detection is done via NameOp _HID inside the recursion.)
      (void)seg;
    }
    return;
  }

  // ----- MethodOp -----
  if (op == AML_METHOD) {
    sc_read(s);
    uint body_end = sc_pkglen(s);
    if (body_end > s->end) body_end = s->end;
    uint name_start = s->pos;
    char seg[5];
    sc_namestring(s, seg);
    s->pos++; // MethodFlags byte

    // Check if this is a battery method at the *current* battery slot.
    // We detect the battery device via _HID earlier, so the battery
    // slot (g_battery_count) is already incremented when we parse _BST.
    // Here we just check by name and attach to the last battery (or a
    // new one) conservatively.
    (void)name_start;

    bool is_bst = (seg[0]=='_' && seg[1]=='B' && seg[2]=='S' && seg[3]=='T');
    bool is_bif = (seg[0]=='_' && seg[1]=='B' &&
                   (seg[2]=='I') && (seg[3]=='F' || seg[3]=='X'));

    if ((is_bst || is_bif) && g_battery_count < MAX_BATTERIES) {
      // If this is the first method for this battery, start a new slot.
      if (g_battery_count == 0 || g_batteries[g_battery_count - 1].present) {
        // mark current slot present and advance only when we see _HID;
        // for now just reuse the last slot or create a new one.
      }
      uint bidx = g_battery_count > 0 ? g_battery_count - 1 : 0;
      battery_dev_t *bat = &g_batteries[bidx];
      if (is_bst && !bat->bst_aml) {
        bat->bst_aml = s->data + s->pos;
        bat->bst_len = body_end - s->pos;
        bat->present = true;
        klogf(LOG_DEBUG, "ACPI: _BST at aml+%u len=%u", s->pos, bat->bst_len);
      } else if (is_bif && !bat->bif_aml) {
        bat->bif_aml = s->data + s->pos;
        bat->bif_len = body_end - s->pos;
        klogf(LOG_DEBUG, "ACPI: _BIF/X at aml+%u len=%u", s->pos, bat->bif_len);
      }
    }
    s->pos = body_end;
    return;
  }

  // ----- NameOp _HID -----
  if (op == AML_NAME) {
    sc_read(s);
    char seg[5];
    sc_namestring(s, seg);
    bool is_hid = (seg[0]=='_' && seg[1]=='H' && seg[2]=='I' && seg[3]=='D');

    // Peek at the value to see if it's PNP0C0A (battery HID).
    // EISAID encoding: 4-byte integer encoding "PNP0C0A" → 0x0AC80D41
    // (common little-endian value seen in ACPI tables)
    bool is_battery_hid = false;
    if (is_hid && s->pos < s->end) {
      ubyte vop = sc_peek(s);
      if (vop == AML_DWORD && s->pos + 5 <= s->end) {
        uint val = *(uint *)(s->data + s->pos + 1);
        // PNP0C0A EISA ID: 0x0AC80D41 (little-endian) or check all common encodings
        // The canonical EISA encoding: compress "PNP0C0A" → 0x0AC80D41
        is_battery_hid = (val == 0x0AC80D41);
      } else if (vop == AML_STRING) {
        // Some BIOSes encode it as a string "PNP0C0A"
        const char *want = "PNP0C0A";
        uint i = s->pos + 1;
        bool match = true;
        for (int j = 0; want[j]; j++, i++)
          if (i >= s->end || s->data[i] != (ubyte)want[j]) { match = false; break; }
        is_battery_hid = match;
      }
    }
    sc_skip_term(s); // skip the value

    if (is_battery_hid && g_battery_count < MAX_BATTERIES) {
      if (g_battery_count == 0 || g_batteries[g_battery_count - 1].bst_aml) {
        g_battery_count++;
        klogf(LOG_INFO, "ACPI: battery device found (idx %u)", g_battery_count - 1);
      }
    }
    return;
  }

  // ----- ExtPrefix: OpRegion or Field -----
  if (op == AML_EXT_PREFIX) {
    sc_read(s);
    if (s->pos >= s->end) return;
    ubyte extop = sc_read(s);

    if (extop == AML_EXT_OPREGION) {
      // NameString RegionSpace RegionOffset RegionLen
      char seg[5]; sc_namestring(s, seg);
      ubyte addrspace = sc_read(s);
      sc_skip_termarg(s); // RegionOffset
      sc_skip_termarg(s); // RegionLen
      // We only record the region name; the bit_base starts at 0 when
      // a Field referencing this region is parsed.
      if (addrspace == ADDRSPACE_EC)
        klogf(LOG_DEBUG, "ACPI: EC OperationRegion %.4s", seg);
      return;
    }

    if (extop == AML_EXT_FIELD) {
      // PkgLength NameString FieldFlags FieldList
      uint body_end = sc_pkglen(s);
      if (body_end > s->end) body_end = s->end;
      char region_name[5]; sc_namestring(s, region_name);
      ubyte field_flags = sc_read(s); (void)field_flags;

      // We assume any Field we encounter under a Device is for the EC.
      // A more correct implementation would look up the region by name
      // and check its address space; for now we add all fields to the
      // global table and they'll only produce real values if ec_read
      // is meaningful on this machine.
      scan_ctx fs = { s->data, s->pos, body_end };
      sc_parse_field_list(&fs, body_end, 0);
      s->pos = body_end;
      return;
    }

    // Other extended opcodes: skip
    s->pos -= 2; // rewind
    sc_skip_term(s);
    return;
  }

  // Anything else: skip
  sc_skip_term(s);
}

static void sc_scan_termlist(scan_ctx *s) {
  while (s->pos < s->end)
    sc_scan_term(s);
}

void aml_scan(const ubyte *aml, uint len) {
  scan_ctx s = { aml, 0, len };
  sc_scan_termlist(&s);
  klogf(LOG_INFO, "ACPI AML: %u battery(ies), %u EC fields", g_battery_count, g_ec_field_count);
}

// ---------------------------------------------------------------------------
// Evaluator
// ---------------------------------------------------------------------------

typedef struct {
  const ubyte *data;
  uint pos;
  uint end;
  ulong locals[8];
  bool  done;
} eval_ctx;

typedef struct {
  bool  valid;
  bool  is_pkg;
  ulong integer;
  ulong pkg_elems[16];
  uint  pkg_count;
} eval_val;

static eval_val EVAL_INVALID = { .valid = false };

static ubyte ev_read(eval_ctx *e) {
  return e->pos < e->end ? e->data[e->pos++] : 0;
}

static ubyte ev_peek(const eval_ctx *e) {
  return e->pos < e->end ? e->data[e->pos] : 0;
}

// Read PkgLength, return absolute end position of the block.
static uint ev_pkglen(eval_ctx *e) {
  if (e->pos >= e->end) return e->pos;
  uint start = e->pos;
  ubyte b0 = ev_read(e);
  uint n = (b0 >> 6) & 3;
  if (n == 0) return start + (b0 & 0x3F);
  uint len = b0 & 0x0F;
  for (uint i = 0; i < n && e->pos < e->end; i++)
    len |= (uint)ev_read(e) << (4 + 8 * i);
  return start + len;
}

static void ev_namestring(eval_ctx *e, char seg[5]) {
  memset8((ubyte *)seg, 0, 5);
  while (e->pos < e->end &&
         (ev_peek(e) == AML_ROOT_CHAR || ev_peek(e) == AML_PARENT_CHAR))
    ev_read(e);
  if (e->pos >= e->end) return;
  ubyte b = ev_peek(e);
  if (b == 0x00) { ev_read(e); return; }
  if (b == AML_DUAL_NAME) {
    ev_read(e);
    for (int i = 0; i < 4; i++) seg[i] = (char)ev_read(e); // first (ignored)
    for (int i = 0; i < 4 && e->pos < e->end; i++) seg[i] = (char)ev_read(e);
    seg[4] = 0; return;
  }
  if (b == AML_MULTI_NAME) {
    ev_read(e);
    ubyte cnt = ev_read(e);
    for (ubyte i = 0; i < cnt && e->pos + 4 <= e->end; i++) {
      for (int j = 0; j < 4; j++) seg[j] = (char)ev_read(e);
    }
    seg[4] = 0; return;
  }
  if ((b >= 'A' && b <= 'Z') || b == '_') {
    for (int i = 0; i < 4 && e->pos < e->end; i++) seg[i] = (char)ev_read(e);
    seg[4] = 0;
  }
}

// Forward declaration
static eval_val ev_eval_term(eval_ctx *e);

// Evaluate a Package opcode (we've already consumed the 0x12 byte).
static eval_val ev_eval_package(eval_ctx *e) {
  uint body_end = ev_pkglen(e);
  if (body_end > e->end) body_end = e->end;
  ubyte num_elems = ev_read(e); // NumElements
  eval_val r = { .valid = true, .is_pkg = true, .pkg_count = 0 };
  for (ubyte i = 0; i < num_elems && e->pos < body_end && r.pkg_count < 16; i++) {
    eval_val elem = ev_eval_term(e);
    r.pkg_elems[r.pkg_count++] = elem.valid ? elem.integer : 0;
  }
  e->pos = body_end;
  return r;
}

// Skip a SuperName target (dest in Store, etc.) without evaluating it.
static void ev_skip_supername(eval_ctx *e) {
  if (e->pos >= e->end) return;
  ubyte b = ev_peek(e);
  if (b >= AML_LOCAL0 && b <= AML_ARG5) { ev_read(e); return; }
  if (b == AML_ROOT_CHAR || b == AML_PARENT_CHAR ||
      b == AML_DUAL_NAME || b == AML_MULTI_NAME ||
      (b >= 'A' && b <= 'Z') || b == '_') {
    char seg[5]; ev_namestring(e, seg); return;
  }
  ev_read(e); // consume unknown
}

static eval_val ev_eval_term(eval_ctx *e) {
  if (e->pos >= e->end || e->done) return EVAL_INVALID;
  ubyte op = ev_read(e);

  switch (op) {
  case AML_ZERO:  return (eval_val){ .valid=true, .integer=0 };
  case AML_ONE:   return (eval_val){ .valid=true, .integer=1 };
  case AML_ONES:  return (eval_val){ .valid=true, .integer=0xFFFFFFFFFFFFFFFFULL };
  case AML_BYTE:  return (eval_val){ .valid=true, .integer=ev_read(e) };
  case AML_WORD: {
    ushort v = e->pos + 2 <= e->end ? *(ushort *)(e->data + e->pos) : 0;
    e->pos += 2;
    return (eval_val){ .valid=true, .integer=v };
  }
  case AML_DWORD: {
    uint v = e->pos + 4 <= e->end ? *(uint *)(e->data + e->pos) : 0;
    e->pos += 4;
    return (eval_val){ .valid=true, .integer=v };
  }
  case AML_QWORD: {
    ulong v = e->pos + 8 <= e->end ? *(ulong *)(e->data + e->pos) : 0;
    e->pos += 8;
    return (eval_val){ .valid=true, .integer=v };
  }
  case AML_NOOP: return EVAL_INVALID;

  // Local0-7
  case AML_LOCAL0: case 0x61: case 0x62: case 0x63:
  case 0x64: case 0x65: case 0x66: case AML_LOCAL7:
    return (eval_val){ .valid=true, .integer=e->locals[op - AML_LOCAL0] };

  // Arg0-5 (we don't have real args, return 0)
  case AML_ARG0: case 0x69: case 0x6A: case 0x6B: case 0x6C: case AML_ARG5:
    return (eval_val){ .valid=true, .integer=0 };

  case AML_PACKAGE: return ev_eval_package(e);

  case AML_RETURN: {
    eval_val v = ev_eval_term(e);
    e->done = true;
    return v;
  }

  // Store: src -> dest.  We evaluate src and store into a Local if dest is one.
  case AML_STORE: {
    eval_val src = ev_eval_term(e);
    ubyte dest = ev_peek(e);
    if (dest >= AML_LOCAL0 && dest <= AML_LOCAL7) {
      ev_read(e);
      e->locals[dest - AML_LOCAL0] = src.valid ? src.integer : 0;
    } else {
      ev_skip_supername(e);
    }
    return src;
  }

  // Binary arithmetic: eval both, return 0 (we can't guarantee correctness
  // without full integer semantics, but we need to consume the bytes).
  case AML_ADD: case AML_SUBTRACT: case AML_MULTIPLY:
  case AML_AND: case AML_NAND: case AML_OR: case AML_NOR: case AML_XOR:
  case AML_SHIFTL: case AML_SHIFTR: {
    eval_val a = ev_eval_term(e);
    eval_val b = ev_eval_term(e);
    ev_skip_supername(e); // Target
    ulong res = 0;
    if (a.valid && b.valid) {
      if (op == AML_ADD)      res = a.integer + b.integer;
      else if (op==AML_SUBTRACT) res = a.integer - b.integer;
      else if (op==AML_AND)   res = a.integer & b.integer;
      else if (op==AML_OR)    res = a.integer | b.integer;
      else if (op==AML_XOR)   res = a.integer ^ b.integer;
      else if (op==AML_SHIFTL) res = a.integer << b.integer;
      else if (op==AML_SHIFTR) res = a.integer >> b.integer;
      else if (op==AML_MULTIPLY) res = a.integer * b.integer;
    }
    return (eval_val){ .valid=true, .integer=res };
  }

  case AML_DIVIDE: {
    eval_val a = ev_eval_term(e);
    eval_val b = ev_eval_term(e);
    ev_skip_supername(e); // Remainder
    ev_skip_supername(e); // Quotient
    ulong res = (a.valid && b.valid && b.integer) ? a.integer / b.integer : 0;
    return (eval_val){ .valid=true, .integer=res };
  }

  case AML_INC: {
    ubyte dest = ev_peek(e);
    if (dest >= AML_LOCAL0 && dest <= AML_LOCAL7) {
      ev_read(e); e->locals[dest - AML_LOCAL0]++;
    } else { ev_skip_supername(e); }
    return EVAL_INVALID;
  }
  case AML_DEC: {
    ubyte dest = ev_peek(e);
    if (dest >= AML_LOCAL0 && dest <= AML_LOCAL7) {
      ev_read(e); e->locals[dest - AML_LOCAL0]--;
    } else { ev_skip_supername(e); }
    return EVAL_INVALID;
  }

  // If/Else/While: evaluate predicate but skip bodies for now
  case AML_IF: {
    uint body_end = ev_pkglen(e);
    if (body_end > e->end) body_end = e->end;
    ev_eval_term(e); // predicate (consume it)
    e->pos = body_end; // skip body
    // Check for matching Else
    if (e->pos < e->end && ev_peek(e) == AML_ELSE) {
      ev_read(e);
      uint else_end = ev_pkglen(e);
      if (else_end > e->end) else_end = e->end;
      e->pos = else_end;
    }
    return EVAL_INVALID;
  }
  case AML_WHILE: {
    uint body_end = ev_pkglen(e);
    if (body_end > e->end) body_end = e->end;
    e->pos = body_end;
    return EVAL_INVALID;
  }
  case AML_ELSE: {
    uint body_end = ev_pkglen(e);
    if (body_end > e->end) body_end = e->end;
    e->pos = body_end;
    return EVAL_INVALID;
  }

  // Name lookup: could be an EC field
  case AML_ROOT_CHAR: case AML_PARENT_CHAR:
  case AML_DUAL_NAME: case AML_MULTI_NAME: {
    e->pos--; // put back
    char seg[5]; ev_namestring(e, seg);
    ec_field_t *f = find_ec_field(seg);
    if (f) return (eval_val){ .valid=true, .integer=read_ec_field(f) };
    return (eval_val){ .valid=true, .integer=0 };
  }

  default:
    if ((op >= 'A' && op <= 'Z') || op == '_') {
      // NameSeg — 3 more bytes
      char seg[5] = { (char)op, 0, 0, 0, 0 };
      for (int i = 1; i < 4 && e->pos < e->end; i++)
        seg[i] = (char)ev_read(e);
      ec_field_t *f = find_ec_field(seg);
      if (f) return (eval_val){ .valid=true, .integer=read_ec_field(f) };
      return (eval_val){ .valid=true, .integer=0 };
    }
    return EVAL_INVALID;
  }
}

// Run the method AML.  Walk terms until done=true or end of stream,
// looking for the last Return value.
static eval_val ev_run_method(const ubyte *aml, uint len) {
  eval_ctx e;
  memset8((ubyte *)&e, 0, sizeof(e));
  e.data = aml;
  e.end  = len;
  eval_val result = EVAL_INVALID;
  while (e.pos < e.end && !e.done) {
    eval_val v = ev_eval_term(&e);
    if (e.done) { result = v; break; }
    (void)v;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool aml_eval_bst(uint idx, aml_bst *out) {
  if (idx >= g_battery_count) return false;
  battery_dev_t *bat = &g_batteries[idx];
  if (!bat->bst_aml) return false;

  eval_val v = ev_run_method(bat->bst_aml, bat->bst_len);
  if (!v.valid || !v.is_pkg || v.pkg_count < 4) return false;

  out->state     = (uint)v.pkg_elems[0];
  out->rate      = (uint)v.pkg_elems[1];
  out->remaining = (uint)v.pkg_elems[2];
  out->voltage   = (uint)v.pkg_elems[3];
  return true;
}

bool aml_eval_bif(uint idx, aml_bif *out) {
  if (idx >= g_battery_count) return false;
  battery_dev_t *bat = &g_batteries[idx];
  if (!bat->bif_aml) return false;

  eval_val v = ev_run_method(bat->bif_aml, bat->bif_len);
  if (!v.valid || !v.is_pkg || v.pkg_count < 5) return false;

  out->power_unit      = (uint)v.pkg_elems[0];
  out->design_capacity = (uint)v.pkg_elems[1];
  out->full_capacity   = (uint)v.pkg_elems[2];
  out->design_voltage  = (uint)v.pkg_elems[4];
  return true;
}
