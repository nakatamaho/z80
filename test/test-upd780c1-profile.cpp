/*
 * Copyright (c) 2026 Nakata Maho
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "z80.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Bus { std::array<unsigned char,65536> memory{}; };
static unsigned char Read(void* p,unsigned short a) { return static_cast<Bus*>(p)->memory[a]; }
static void Write(void* p,unsigned short a,unsigned char v) { static_cast<Bus*>(p)->memory[a]=v; }
static unsigned char Input(void*,unsigned short) { return 0xff; }
static void Output(void*,unsigned short,unsigned char) {}

static unsigned short Expected(unsigned af,unsigned op) {
    unsigned f=af&255u;
    return static_cast<unsigned short>((af&0xff00u)|(f&0xc4u)|((af>>8)&0x28u)|
                                      (op==0x37u?1u:(f&1u?0x10u:1u)));
}

struct Harness {
    Bus bus;
    Z80 cpu;
    Harness():cpu(Read,Write,Input,Output,&bus) {
        cpu.setFlagProfile(Z80::FlagProfile::Upd780C1);
        if(cpu.getFlagProfile()!=Z80::FlagProfile::Upd780C1) std::exit(1);
    }
    unsigned short Run(const std::vector<unsigned char>& code) {
        cpu.reg=Z80::Register{};
        cpu.reg.PC=0x100;
        cpu.reg.SP=0x7eff;
        cpu.reg.IX=0x1357;
        cpu.reg.IY=0x2468;
        for(std::size_t i=0;i<code.size();++i) bus.memory[0x100u+i]=code[i];
        const unsigned end=0x100u+static_cast<unsigned>(code.size());
        for(unsigned guard=0;guard<32&&cpu.reg.PC!=end;++guard) cpu.execute(1);
        const unsigned short af=static_cast<unsigned short>((cpu.reg.pair.A<<8)|cpu.reg.pair.F);
        const unsigned short de=static_cast<unsigned short>((cpu.reg.pair.D<<8)|cpu.reg.pair.E);
        if(cpu.reg.PC!=end||cpu.reg.SP!=0x7eff||cpu.reg.IX!=0x1357||cpu.reg.IY!=0x2468||af!=de) {
            std::fputs("UPD780C1_EXPORT_OR_FLOW\n",stderr);std::exit(1);
        }
        return de;
    }
};

int main() {
    static_assert(static_cast<int>(Z80::FlagProfile::Zilog)==0,"existing enum value");
    static_assert(static_cast<int>(Z80::FlagProfile::Upd9002)==1,"existing enum value");
    Harness h;
    unsigned cases=0;
    const unsigned prefixes[]={0,0xdd,0xfd};
#ifdef Z80_NO_EXCEPTION
    const unsigned prefixCount=3;
#else
    // Default strict builds intentionally reject ignored DD/FD opcodes.
    const unsigned prefixCount=1;
#endif
    for(unsigned af=0;af<65536;++af) for(unsigned which=0;which<2;++which) {
        const unsigned op=which?0x3fu:0x37u;
        for(unsigned p=0;p<prefixCount;++p) {
            std::vector<unsigned char> code={0x01,static_cast<unsigned char>(af),static_cast<unsigned char>(af>>8),0xc5,0xf1,static_cast<unsigned char>(prefixes[p]),static_cast<unsigned char>(op),0xf5,0xd1};
            const unsigned short actual=h.Run(code),expected=Expected(af,op);
            if(actual!=expected) {
                std::fprintf(stderr,"UPD780C1_AF af=%04X op=%02X prefix=%02X got=%04X expected=%04X\n",af,op,prefixes[p],actual,expected);return 1;
            }
            ++cases;
        }
    }
    unsigned cpCases=0;
#ifdef Z80_NO_EXCEPTION
    const unsigned modeCount=4;
#else
    const unsigned modeCount=2;
#endif
    for(unsigned a: {0u,0x28u}) for(unsigned mode=0;mode<modeCount;++mode) for(unsigned op: {0x37u,0x3fu}) {
        std::vector<unsigned char> code={0x3e,static_cast<unsigned char>(a),0x06,0x28};
        if(mode==0) code.push_back(0); // NOP before CP: CP is immediately before IUT.
        code.push_back(0xb8); // CP B
        if(mode) code.push_back(mode==1?0:mode==2?0xdd:0xfd);
        code.push_back(static_cast<unsigned char>(op));code.push_back(0xf5);code.push_back(0xd1);
        const unsigned short expected=Expected(a?0x286au:0x00bbu,op);
        if(h.Run(code)!=expected) {std::fputs("UPD780C1_CP_BOUNDARY\n",stderr);return 1;}
        ++cpCases;
    }
    std::printf("UPD780C1_PASS software_AF_cases=%u CP_boundary_cases=%u (not exhaustive physical coverage)\n",cases,cpCases);
    return 0;
}
