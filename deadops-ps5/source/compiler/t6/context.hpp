// Copyright 2023 xensik. All rights reserved.
//
// Use of this source code is governed by a GNU GPLv3 license
// that can be found in the LICENSE file.

#pragma once

namespace xsk::arc::t6
{

class context : public arc::context
{
    t6::assembler assembler_;
    t6::compiler compiler_;

public:
    // legacy (PS3, Xbox 360, Wii U) is big-endian PowerPC and shares the PC
    // opcode table; ps4 is little-endian with its own opcode numbering and
    // 64 bit operand slots.
    explicit context(platform sys = platform::pc);
    explicit context(bool big_endian) : context(big_endian ? platform::legacy : platform::pc) {}

    void init(build mode, read_cb_type callback);
    void cleanup();

    auto assembler() -> arc::assembler& { return assembler_; }
    auto compiler() -> arc::compiler& { return compiler_; }
};

} // namespace xsk::arc::t6
