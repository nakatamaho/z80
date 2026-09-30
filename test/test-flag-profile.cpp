// FlagProfile test: Zilog (default) and Upd9002 (NEC
// uPD9002 Z80 emulation mode, rules R1-R7). F is loaded with PUSH BC / POP AF
// and captured with PUSH AF / POP DE right after the instruction under test.
#include "z80.hpp"

struct Probe {
    const char* name;
    unsigned short af;
    unsigned short bc;
    unsigned short hl;
    unsigned char body[8];
    int bodySize;
    unsigned char a;        // expected A (both profiles)
    unsigned char fZilog;   // expected F, Zilog
    unsigned char fUpd9002; // expected F, Upd9002
};

static const Probe probes[] = {
    {"XOR A", 0x5A00, 0x0000, 0x0000, {0xAF}, 1, 0x00, 0x44, 0x44},
    {"AND $0F", 0xFF00, 0x0000, 0x0000, {0xE6, 0x0F}, 2, 0x0F, 0x1C, 0x04},       // R1, R2
    {"BIT 0,A", 0x0100, 0x0000, 0x0000, {0xCB, 0x47}, 2, 0x01, 0x10, 0x00},       // R3
    {"RLCA", 0x80D7, 0x0000, 0x0000, {0x07}, 1, 0x01, 0xC5, 0xD7},                // R4
    {"LDI (BC=1)", 0x00D7, 0x0001, 0x4000, {0xED, 0xA0}, 2, 0x00, 0xC1, 0xD3},    // R5
    {"LDI (BC=2)", 0x00D7, 0x0002, 0x4000, {0xED, 0xA0}, 2, 0x00, 0xC5, 0xD7},    // R5
    {"ADD HL,BC (H set)", 0x0000, 0x0001, 0x0FFF, {0x09}, 1, 0x00, 0x10, 0x00},   // R6
    {"ADD HL,BC (H kept)", 0x0010, 0x0001, 0x0001, {0x09}, 1, 0x00, 0x00, 0x10},  // R6
    {"ADC HL,BC", 0x0000, 0x0001, 0x000F, {0xED, 0x4A}, 2, 0x00, 0x00, 0x10},     // R7
    {"SBC HL,BC", 0x0000, 0x0001, 0x0010, {0xED, 0x42}, 2, 0x00, 0x02, 0x12},     // R7
    {"POP AF ($FFFF)", 0x0000, 0xFFFF, 0x0000, {0xC5, 0xF1}, 2, 0xFF, 0xFF, 0xD7}, // R1
    {"EX AF,AF' x2", 0x0000, 0xFFFF, 0x0000, {0xC5, 0xF1, 0x08, 0x08}, 4, 0xFF, 0xFF, 0xD7}, // R1
    {"CPL", 0x0000, 0x0000, 0x0000, {0x2F}, 1, 0xFF, 0x3A, 0x00},                  // R8
    {"CPL (F=$D7)", 0x00D7, 0x0000, 0x0000, {0x2F}, 1, 0xFF, 0xFF, 0xD7},          // R8
    {"SCF (H,N set)", 0x0012, 0x0000, 0x0000, {0x37}, 1, 0x00, 0x01, 0x13},        // R9
    {"CCF (C set)", 0x0013, 0x0000, 0x0000, {0x3F}, 1, 0x00, 0x10, 0x12},          // R10
    {"CCF (C clear)", 0x0012, 0x0000, 0x0000, {0x3F}, 1, 0x00, 0x01, 0x13},        // R10
    {"DAA A=$00", 0x0000, 0x0000, 0x0000, {0x27}, 1, 0x00, 0x44, 0x40},            // R11
    {"DAA A=$7A", 0x7A00, 0x0000, 0x0000, {0x27}, 1, 0x80, 0x90, 0x94},            // R11
};
// DAA cases whose A result also differs between the profiles (R11: with H=1
// the high step needs A > $9F instead of A > $99).
static const Probe daaHighProbes[] = {
    {"DAA A=$9A H", 0x9A10, 0x0000, 0x0000, {0x27}, 1, 0x00, 0x55, 0x00},
    {"DAA A=$9A H N", 0x9A12, 0x0000, 0x0000, {0x27}, 1, 0x00, 0x23, 0x00},
};
static const unsigned char daaHighA[][2] = {{0x00, 0xA0}, {0x34, 0x94}};
static const unsigned char daaHighFUpd9002[] = {0x90, 0x92};

int main()
{
    unsigned char memory[65536];
    int failures = 0;
    for (int profile = 0; profile < 2; profile++) {
        for (const Probe& probe : probes) {
            memset(memory, 0, sizeof(memory));
            unsigned char* p = memory;
            *p++ = 0x01; // LD BC,af
            *p++ = (unsigned char)probe.af;
            *p++ = (unsigned char)(probe.af >> 8);
            *p++ = 0xC5; // PUSH BC
            *p++ = 0xF1; // POP AF
            *p++ = 0x01; // LD BC,bc
            *p++ = (unsigned char)probe.bc;
            *p++ = (unsigned char)(probe.bc >> 8);
            *p++ = 0x21; // LD HL,hl
            *p++ = (unsigned char)probe.hl;
            *p++ = (unsigned char)(probe.hl >> 8);
            *p++ = 0x11; // LD DE,$5000
            *p++ = 0x00;
            *p++ = 0x50;
            memcpy(p, probe.body, (size_t)probe.bodySize);
            p += probe.bodySize;
            *p++ = 0xF5; // PUSH AF
            *p++ = 0xD1; // POP DE
            unsigned short end = (unsigned short)(p - memory);
            Z80 z80([&memory](void* arg, unsigned short addr) { return memory[addr]; },
                    [&memory](void* arg, unsigned short addr, unsigned char value) { memory[addr] = value; },
                    [](void* arg, unsigned short port) { return (unsigned char)0xFF; },
                    [](void* arg, unsigned short port, unsigned char value) {},
                    &z80);
            z80.setFlagProfile(profile ? Z80::FlagProfile::Upd9002 : Z80::FlagProfile::Zilog);
            z80.reg.SP = 0xF000;
            while (z80.reg.PC != end) z80.execute(1);
            unsigned char expectedF = profile ? probe.fUpd9002 : probe.fZilog;
            bool ok = z80.reg.pair.A == probe.a && z80.reg.pair.E == expectedF;
            printf("%s: %-8s %-20s A=$%02X F=$%02X (expected A=$%02X F=$%02X)\n", ok ? "OK" : "NG",
                   profile ? "Upd9002" : "Zilog", probe.name, z80.reg.pair.A, z80.reg.pair.E, probe.a, expectedF);
            if (!ok) failures++;
        }
    }
    for (int profile = 0; profile < 2; profile++) {
        for (int i = 0; i < 2; i++) {
            const Probe& probe = daaHighProbes[i];
            memset(memory, 0, sizeof(memory));
            const unsigned char code[] = {0x01, (unsigned char)probe.af, (unsigned char)(probe.af >> 8), // LD BC,af
                                          0xC5, 0xF1, 0x27, 0xF5, 0xD1};                               // PUSH BC, POP AF, DAA, PUSH AF, POP DE
            memcpy(memory, code, sizeof(code));
            Z80 z80([&memory](void* arg, unsigned short addr) { return memory[addr]; },
                    [&memory](void* arg, unsigned short addr, unsigned char value) { memory[addr] = value; },
                    [](void* arg, unsigned short port) { return (unsigned char)0xFF; },
                    [](void* arg, unsigned short port, unsigned char value) {},
                    &z80);
            z80.setFlagProfile(profile ? Z80::FlagProfile::Upd9002 : Z80::FlagProfile::Zilog);
            z80.reg.SP = 0xF000;
            while (z80.reg.PC != sizeof(code)) z80.execute(1);
            unsigned char expectedA = daaHighA[i][profile];
            unsigned char expectedF = profile ? daaHighFUpd9002[i] : probe.fZilog;
            bool ok = z80.reg.pair.A == expectedA && z80.reg.pair.E == expectedF;
            printf("%s: %-8s %-20s A=$%02X F=$%02X (expected A=$%02X F=$%02X)\n", ok ? "OK" : "NG",
                   profile ? "Upd9002" : "Zilog", probe.name, z80.reg.pair.A, z80.reg.pair.E, expectedA, expectedF);
            if (!ok) failures++;
        }
    }
    // R12/R13: DD CB d xx with a register operand (low three bits != 6)
    const struct {
        const char* name;
        unsigned char op;
        unsigned char zilogB, zilogMem, upd9002B, upd9002Mem;
        unsigned char zilogF, upd9002F;
    } cbCases[] = {
        {"RES 0,(IX+2),B", 0x80, 0x80, 0x80, 0x0A, 0x81, 0x00, 0x00}, // B=0B, (IX+2)=81
        {"SET 7,(IX+2),B", 0xF8, 0x81, 0x81, 0x8B, 0x81, 0x00, 0x00},
        {"BIT 0,(IX+2) [B]", 0x40, 0x0B, 0x81, 0x0B, 0x81, 0x10, 0x00}, // Zilog tests (IX+2), uPD9002 tests B
        {"BIT 2,(IX+2) [B]", 0x50, 0x0B, 0x81, 0x0B, 0x81, 0x54, 0x44},
    };
    for (int profile = 0; profile < 2; profile++) {
        for (const auto& c : cbCases) {
            memset(memory, 0, sizeof(memory));
            const unsigned char code[] = {0x06, 0x0B, 0xDD, 0xCB, 0x02, c.op, 0xF5, 0xD1}; // LD B,0B; op; PUSH AF; POP DE
            memcpy(memory, code, sizeof(code));
            memory[0x5002] = 0x81;
            Z80 z80([&memory](void* arg, unsigned short addr) { return memory[addr]; },
                    [&memory](void* arg, unsigned short addr, unsigned char value) { memory[addr] = value; },
                    [](void* arg, unsigned short port) { return (unsigned char)0xFF; },
                    [](void* arg, unsigned short port, unsigned char value) {},
                    &z80);
            z80.setFlagProfile(profile ? Z80::FlagProfile::Upd9002 : Z80::FlagProfile::Zilog);
            z80.reg.SP = 0xF000;
            z80.reg.IX = 0x5000;
            z80.reg.pair.A = 0x00;
            z80.reg.pair.F = 0x00;
            while (z80.reg.PC != sizeof(code)) z80.execute(1);
            unsigned char b = profile ? c.upd9002B : c.zilogB;
            unsigned char m = profile ? c.upd9002Mem : c.zilogMem;
            unsigned char f = profile ? c.upd9002F : c.zilogF;
            bool ok = z80.reg.pair.B == b && memory[0x5002] == m && z80.reg.pair.E == f;
            printf("%s: %-8s %-20s B=$%02X (IX+2)=$%02X F=$%02X (expected B=$%02X (IX+2)=$%02X F=$%02X)\n", ok ? "OK" : "NG",
                   profile ? "Upd9002" : "Zilog", c.name, z80.reg.pair.B, memory[0x5002], z80.reg.pair.E, b, m, f);
            if (!ok) failures++;
        }
    }
    return failures ? -1 : 0;
}
