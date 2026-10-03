# SPDX-License-Identifier: GPL-3.0-or-later
"""Just enough ELF64 (little-endian) reading for isa-score: executable
sections, function symbols and the E2K ISA version in e_flags."""

import struct
from dataclasses import dataclass

SHF_EXECINSTR = 0x4
SHT_SYMTAB, SHT_DYNSYM = 2, 11
STT_FUNC = 2
ET_DYN = 3


@dataclass
class Section:
    name: str
    addr: int
    data: bytes


@dataclass
class Symbol:
    name: str
    addr: int
    size: int


class Elf:
    def __init__(self, path):
        raw = open(path, "rb").read()
        if raw[:4] != b"\x7fELF" or raw[4] != 2 or raw[5] != 1:
            raise ValueError(f"{path}: not a little-endian ELF64 file")
        (self.e_type, self.e_machine, _, self.entry, _, shoff, self.e_flags,
         _, _, _, shentsize, shnum, shstrndx) = struct.unpack_from(
            "<HHIQQQIHHHHHH", raw, 16)
        shdrs = [struct.unpack_from("<IIQQQQIIQQ", raw, shoff + i * shentsize)
                 for i in range(shnum)]
        strtab = shdrs[shstrndx]
        names = raw[strtab[4]:strtab[4] + strtab[5]]

        def cstr(buf, off):
            return buf[off:buf.index(b"\0", off)].decode()

        self.sections = []
        self.symbols = {}
        for name, typ, flags, addr, off, size, link, _, _, entsize in shdrs:
            if flags & SHF_EXECINSTR and typ != 8:  # not NOBITS
                self.sections.append(Section(cstr(names, name), addr,
                                             raw[off:off + size]))
            if typ in (SHT_SYMTAB, SHT_DYNSYM):
                strs = shdrs[link]
                sstr = raw[strs[4]:strs[4] + strs[5]]
                for i in range(size // entsize):
                    st_name, info, _, shndx, value, ssize = struct.unpack_from(
                        "<IBBHQQ", raw, off + i * entsize)
                    if info & 0xf == STT_FUNC and shndx != 0 and value:
                        self.symbols.setdefault(value, Symbol(
                            cstr(sstr, st_name), value, ssize))
        self.pie = self.e_type == ET_DYN

    # CPU model codes in e_flags and their ISA versions (spec/abi/elf.md).
    MODELS = {19: 4, 20: 4, 21: 6, 22: 6, 23: 6, 24: 7, 25: 7}

    def iset(self):
        """The ISA version from e_flags' top byte (spec/abi/elf.md), or
        None when it is unknown."""
        v = self.e_flags >> 24
        return v if 2 <= v <= 7 else self.MODELS.get(v)
