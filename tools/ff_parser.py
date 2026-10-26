"""
T6 (Black Ops II) Fast File (.ff) Parser
Parses TAffu100 format files, finds and decompresses zlib streams,
and locates GSC script assets for mod injection.
"""
import struct
import zlib
import sys
import os
from pathlib import Path


class T6FastFile:
    MAGIC = b"TAffu100"

    def __init__(self, filepath):
        self.filepath = Path(filepath)
        self.data = self.filepath.read_bytes()
        self.name = self.filepath.stem
        self.header = {}
        self.zlib_streams = []
        self.decompressed_chunks = []
        self.gsc_assets = []
        self._parse_header()

    def _parse_header(self):
        if self.data[:8] != self.MAGIC:
            raise ValueError(f"Invalid magic: {self.data[:8]!r}, expected {self.MAGIC!r}")
        self.header["magic"] = self.data[:8].decode("ascii")
        self.header["header_size"] = struct.unpack_from("<I", self.data, 8)[0]
        self.header["section_count"] = struct.unpack_from("<I", self.data, 12)[0]
        print(f"[*] File: {self.filepath.name}")
        print(f"    Size: {len(self.data):,} bytes")
        print(f"    Header size: 0x{self.header['header_size']:X} ({self.header['header_size']})")
        print(f"    Section count: {self.header['section_count']}")

    def find_zlib_streams(self):
        """Find all zlib-compressed streams in the file."""
        markers = [(0x78, 0x9C), (0x78, 0x01), (0x78, 0xDA), (0x78, 0x5E)]
        self.zlib_streams = []

        for i in range(len(self.data) - 1):
            for m1, m2 in markers:
                if self.data[i] == m1 and self.data[i + 1] == m2:
                    self.zlib_streams.append(i)
                    break

        print(f"[*] Found {len(self.zlib_streams)} potential zlib streams")
        return self.zlib_streams

    def decompress_streams(self, max_streams=None):
        """Try to decompress all found zlib streams."""
        if not self.zlib_streams:
            self.find_zlib_streams()

        count = max_streams or len(self.zlib_streams)
        success = 0
        self.decompressed_chunks = []

        for i, offset in enumerate(self.zlib_streams[:count]):
            try:
                decompressor = zlib.decompressobj()
                chunk = decompressor.decompress(self.data[offset:offset + 1024 * 1024])
                remaining = decompressor.unused_data
                compressed_size = offset + len(self.data[offset:]) - len(remaining) - offset
                self.decompressed_chunks.append({
                    "offset": offset,
                    "compressed_size": len(self.data[offset:]) - len(remaining),
                    "decompressed_size": len(chunk),
                    "data": chunk,
                })
                success += 1
            except zlib.error:
                pass
            except Exception:
                pass

        print(f"[*] Successfully decompressed {success}/{count} streams")
        return self.decompressed_chunks

    def find_gsc_scripts(self):
        """Search decompressed data for GSC script content."""
        gsc_patterns = [
            b"scripts/zm/",
            b"maps/mp/zm_",
            b"maps/mp/zombies/",
            b"_zm_",
            b"#include",
            b"thread ",
            b"self ",
            b"level.",
            b"player ",
            b"waittill",
            b"zombie",
            b"getplayers",
            b"scripts/zm/zm_",
            b"maps/mp/gametypes_zm/",
            b"animscripts/",
            b"common_scripts/",
        ]

        all_data = b""
        for chunk in self.decompressed_chunks:
            all_data += chunk["data"]

        print(f"\n[*] Searching {len(all_data):,} bytes of decompressed data for GSC patterns...")
        for pattern in gsc_patterns:
            count = all_data.count(pattern)
            if count > 0:
                idx = all_data.find(pattern)
                context_start = max(0, idx - 10)
                context_end = min(len(all_data), idx + len(pattern) + 60)
                context = all_data[context_start:context_end]
                safe_context = context.decode("ascii", errors="replace").replace("\x00", "\\0")
                print(f"    Found '{pattern.decode()}' x{count} — first at byte {idx}: ...{safe_context}...")

    def find_script_strings(self):
        """Search for readable strings that indicate GSC script locations."""
        all_data = bytearray()
        for chunk in self.decompressed_chunks:
            all_data.extend(chunk["data"])

        strings = []
        current = bytearray()
        for b in all_data:
            if 0x20 <= b < 0x7F:
                current.append(b)
            else:
                if len(current) >= 8:
                    s = current.decode("ascii")
                    if any(kw in s.lower() for kw in [
                        "script", "zm", "zombie", "gsc", "maps/",
                        "common", "include", "thread", "func",
                        "player", "weapon", "perk", "level",
                        "spawn", "health", "ammo", "point",
                        "round", "powerup", "game"
                    ]):
                        strings.append(s)
                current = bytearray()

        unique = sorted(set(strings))
        print(f"\n[*] Found {len(unique)} unique script-related strings:")
        for s in unique[:100]:
            print(f"    {s}")
        if len(unique) > 100:
            print(f"    ... and {len(unique) - 100} more")
        return unique

    def scan_raw_strings(self, min_len=10):
        """Scan the raw (potentially encrypted) file for any readable strings."""
        strings = []
        current = bytearray()
        for i, b in enumerate(self.data):
            if 0x20 <= b < 0x7F:
                current.append(b)
            else:
                if len(current) >= min_len:
                    strings.append((i - len(current), current.decode("ascii")))
                current = bytearray()
        return strings

    def extract_decompressed(self, output_dir):
        """Write all decompressed chunks to files for analysis."""
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        for i, chunk in enumerate(self.decompressed_chunks):
            outpath = output_dir / f"chunk_{i:04d}_0x{chunk['offset']:X}.bin"
            outpath.write_bytes(chunk["data"])

        print(f"[*] Extracted {len(self.decompressed_chunks)} chunks to {output_dir}")


def analyze_file(filepath):
    ff = T6FastFile(filepath)
    ff.find_zlib_streams()
    ff.decompress_streams()

    if ff.decompressed_chunks:
        ff.find_gsc_scripts()
        ff.find_script_strings()

        total_decompressed = sum(c["decompressed_size"] for c in ff.decompressed_chunks)
        print(f"\n[*] Summary:")
        print(f"    Total decompressed: {total_decompressed:,} bytes")
        print(f"    Largest chunk: {max(c['decompressed_size'] for c in ff.decompressed_chunks):,} bytes")
    else:
        print("[!] No decompressible zlib streams found")
        raw_strings = ff.scan_raw_strings()
        print(f"[*] Raw readable strings in file: {len(raw_strings)}")
        for offset, s in raw_strings[:30]:
            print(f"    0x{offset:X}: {s}")

    return ff


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python ff_parser.py <path_to_ff_file>")
        sys.exit(1)
    analyze_file(sys.argv[1])
