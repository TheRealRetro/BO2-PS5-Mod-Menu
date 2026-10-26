"""Re-apply the ORBIS edits to the vendored gsc-tool T6 backend.

The files under t6/ and utils/ are copied unmodified from gsc-tool so they stay
easy to re-sync. Everything the SDK's pre-C++20 library cannot compile is
patched here instead of by hand, so a fresh copy can be fixed by rerunning this.

Each edit is idempotent: running twice changes nothing.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

EDITS = [
    # The one fmt::format call, formatting a line number as uppercase hex.
    ("t6/misc/location.hpp",
     'return fmt::format("loc_{:X}", begin.line);',
     'return "loc_" + hex_upper(begin.line);'),

    # A hex dump helper; fmt::format_to has no equivalent in this library.
    ("utils/reader.cpp",
     'fmt::format_to(std::back_insert_iterator(data), "{:02X} ", '
     '*reinterpret_cast<u8 const*>(data_ + i));',
     'char byte[4];\n'
     '        std::snprintf(byte, sizeof(byte), "%02X ", '
     '*reinterpret_cast<u8 const*>(data_ + i));\n'
     '        data += byte;'),

    # string::starts_with
    ("t6/lexer.cpp",
     'if (std::string_view(buffer_.data, buffer_.length).starts_with("_hash_"))',
     'if (starts_with(std::string_view(buffer_.data, buffer_.length), "_hash_"))'),
    ("t6/resolver.cpp",
     'if (str.starts_with("_id_") || str.starts_with("_func_") || str.starts_with("_meth_"))',
     'if (starts_with(str, "_id_") || starts_with(str, "_func_") || starts_with(str, "_meth_"))'),

    # unordered_set::contains
    ("t6/assembler.cpp",
     'if (!stringlist_.contains(data))',
     'if (!contains(stringlist_, data))'),
    ("t6/misc/nodetree.cpp",
     'return value.starts_with("_hash_") ? value : ("\\"" + value + "\\"");',
     'return starts_with(value, "_hash_") ? value : ("\\"" + value + "\\"");'),

    # std::regex is absent from this library; nothing on the compile path calls
    # parse_code, so it goes rather than being reimplemented.
    ("utils/string.hpp",
     "    static auto parse_code(std::string& line) -> std::vector<std::string>;\n",
     ""),
    # This library's unordered_map::insert only takes a const value_type&, so it
    # copies - and ast::expr is move-only. emplace forwards instead.
    ("t6/compiler.cpp",
     "constants_.insert({ stmt->lvalue->value, std::move(stmt->rvalue) });",
     "constants_.emplace(stmt->lvalue->value, std::move(stmt->rvalue));"),

    # --- compile-only: drop the decompiler and disassembler ------------------
    # Nothing here decompiles, but t6::context held one of each as a member, so
    # both objects were linked in just for their vtables.
    ("t6/context.hpp", "    t6::disassembler disassembler_;\n", ""),
    ("t6/context.hpp", "    t6::decompiler decompiler_;\n", ""),
    ("t6/context.hpp",
     "    auto disassembler() -> arc::disassembler& { return disassembler_; }\n", ""),
    ("t6/context.hpp",
     "    auto decompiler() -> arc::decompiler& { return decompiler_; }\n", ""),
    ("t6/context.cpp", "    disassembler_.set_endian(big_endian);\n", ""),
    ("t6/context.cpp", "    disassembler_.set_platform(sys);\n", ""),
    ("t6/misc/interfaces/context.hpp",
     "    virtual auto disassembler() -> disassembler& = 0;\n", ""),
    ("t6/misc/interfaces/context.hpp",
     "    virtual auto decompiler() -> decompiler& = 0;\n", ""),
]


def drop_dead_definitions():
    """fs_to_game_path is the only std::filesystem user and parse_code the only
    std::regex user; neither is reachable from the compile path."""
    n = 0

    hpp = os.path.join(HERE, "t6/resolver.hpp")
    text = open(hpp, encoding="utf-8").read()
    decl = "    static auto fs_to_game_path(const std::filesystem::path& file) -> std::filesystem::path;\n"
    if decl in text:
        open(hpp, "w", encoding="utf-8").write(text.replace(decl, ""))
        n += 1

    cpp = os.path.join(HERE, "utils/string.cpp")
    text = open(cpp, encoding="utf-8").read()
    m = re.search(r"auto string::parse_code\(.*?\n\}\n\n", text, re.S)
    if m:
        open(cpp, "w", encoding="utf-8").write(text[:m.start()] + text[m.end():])
        n += 1

    cpp = os.path.join(HERE, "t6/resolver.cpp")
    text = open(cpp, encoding="utf-8").read()
    m = re.search(r"auto resolver::fs_to_game_path\(.*?\n\}\n\n", text, re.S)
    if m:
        open(cpp, "w", encoding="utf-8").write(text[:m.start()] + text[m.end():])
        n += 1
    return n


def main():
    applied, already = 0, 0
    for rel, old, new in EDITS:
        path = os.path.join(HERE, rel)
        text = open(path, encoding="utf-8").read()
        if old not in text:
            # a deletion leaves nothing to look for, so absence of the original
            # is the only signal that it already ran
            if new and new in text or not new:
                already += 1
                continue
            sys.exit("could not find in %s:\n  %s" % (rel, old))
        open(path, "w", encoding="utf-8").write(text.replace(old, new))
        print("patched %s" % rel)
        applied += 1

    dropped = drop_dead_definitions()
    print("removed %d dead definitions (fs_to_game_path, parse_code)" % dropped)
    print("%d edits applied, %d already in place" % (applied, already))


if __name__ == "__main__":
    main()
