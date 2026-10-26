// Copyright 2023 xensik. All rights reserved.
//
// Use of this source code is governed by a GNU GPLv3 license
// that can be found in the LICENSE file.

#include "stdinc.hpp"
#include "t6.hpp"

namespace xsk::arc::t6
{

context::context(platform sys)
{
    const auto big_endian = big_endian_for(sys);

    assembler_.set_endian(big_endian);

    // Drives the opcode translation, the header magic and the operand widths.
    assembler_.set_platform(sys);
}

void context::init(build mode, read_cb_type callback)
{
    compiler_.mode(mode);
    resolver::init(callback);
}

void context::cleanup()
{
    resolver::cleanup();
}

} // namespace xsk::arc::t6
