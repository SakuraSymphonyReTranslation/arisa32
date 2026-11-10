#!/usr/bin/env python3
"""Run the original DC2TY save readers in an isolated x86 Unicorn instance.

Usage:
    python tools/verify_dc2ty_native_save.py --exe /path/to/DC2TY.exe save.dat ...

Requires Python packages pefile and unicorn. The EXE must match EXE_SHA256.
Inputs are read only. CRT file calls use supplied bytes in memory; no native
code, Windows APIs, GUI, or original game save writes run on the host.

The metadata reader must return success. The loader must copy every save byte
to its runtime buffer before reaching 0x426580, where emulation stops. This
checks native header/length acceptance and copying, not scene restoration,
sprite rendering, audio, script checkpoints, or .dat.arisa companions. Native
readers request only SAVE_SIZE bytes and can accept a larger file's prefix;
this verifier additionally requires each supplied export to be exactly that
size. See docs/dc2ty-native-save-reference.md for the disassembly evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

try:
    import pefile
    from unicorn import Uc, UcError, UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
except ImportError as error:
    raise SystemExit("Install Python packages pefile and unicorn to run this verifier.") from error


EXE_SHA256 = "21988ca3227d494ad8fd816fd6363670b1a6245f82ac28314c4bb4e8353d19bd"
SAVE_SIZE = 0x2944
METADATA = 0x426920
LOADER = 0x426860
RESTORE = 0x426580
READ_BUFFER = 0xACE110
RUNTIME_BUFFER = 0xAD0A58
DIRECTORY_BUFFER = 0xA22264
SPRINTF = 0x42C873
OPEN = 0x42C294
READ = 0x42BED0
CLOSE = 0x42C1E1
STACK = 0x10000000
STACK_SIZE = 0x10000
RETURN = 0x20000000
VIRTUAL_DIRECTORY = b"C:\\arisa32-probe\\Savegame"
SENTINEL = b"\xD3" * SAVE_SIZE


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


class NativeReaders:
    def __init__(self, exe: Path):
        data = exe.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        require(digest == EXE_SHA256, f"Unsupported EXE SHA-256: {digest}")
        pe = pefile.PE(data=data, fast_load=True)
        require(pe.FILE_HEADER.Machine == 0x14C, "Expected an x86 PE executable")
        self.base = pe.OPTIONAL_HEADER.ImageBase
        require(self.base == 0x400000, "Unexpected executable image base")
        self.size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.image = pe.get_memory_mapped_image()
        pe.close()

    def run(self, entry: int, payload: bytes, *, present: bool = True, slot: int = 2) -> dict:
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(self.base, self.size)
        uc.mem_write(self.base, self.image)
        uc.mem_map(STACK, STACK_SIZE)
        uc.mem_map(RETURN, 0x1000)
        uc.mem_write(DIRECTORY_BUFFER, VIRTUAL_DIRECTORY + b"\0")
        uc.mem_write(RUNTIME_BUFFER, SENTINEL)
        stack_top = STACK + STACK_SIZE - 0x100
        uc.mem_write(stack_top, struct.pack("<II", RETURN, slot))
        uc.reg_write(UC_X86_REG_ESP, stack_top)
        calls: list[str] = []
        stopped = ""
        expected_path = VIRTUAL_DIRECTORY + b"\\dc2ty" + f"{slot:02d}".encode("ascii") + b".dat"

        def word(address: int) -> int:
            return struct.unpack("<I", uc.mem_read(address, 4))[0]

        def argument(index: int) -> int:
            return word(uc.reg_read(UC_X86_REG_ESP) + 4 * (index + 1))

        def string(address: int) -> bytes:
            value = bytearray()
            for index in range(260):
                char = uc.mem_read(address + index, 1)[0]
                if not char:
                    return bytes(value)
                value.append(char)
            raise ValueError("Unterminated native path/string")

        def cdecl_return(value: int) -> None:
            esp = uc.reg_read(UC_X86_REG_ESP)
            uc.reg_write(UC_X86_REG_EAX, value & 0xFFFFFFFF)
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EIP, word(esp))

        def hook(_uc: Uc, address: int, _size: int, _user_data: object) -> None:
            nonlocal stopped
            if address == RETURN:
                stopped = "returned"
                uc.emu_stop()
            elif address == RESTORE:
                require(entry == LOADER, "Metadata reader unexpectedly called scene restoration")
                stopped = "restore"
                uc.emu_stop()
            elif address == SPRINTF:
                require(not calls, "Unexpected repeated path formatting")
                require(string(argument(1)) == b"%s\\%s%02d.dat", "Unexpected native filename format")
                require(string(argument(2)) == VIRTUAL_DIRECTORY, "Unexpected save directory")
                require(string(argument(3)) == b"dc2ty" and argument(4) == slot, "Unexpected save prefix/slot")
                uc.mem_write(argument(0), expected_path + b"\0")
                calls.append("sprintf")
                cdecl_return(len(expected_path))
            elif address == OPEN:
                require(calls == ["sprintf"], "Unexpected native open order")
                require(string(argument(0)) == expected_path and argument(1) == 0x8000, "Unexpected native open path/flags")
                calls.append("open")
                cdecl_return(42 if present else -1)
            elif address == READ:
                require(present and calls == ["sprintf", "open"], "Unexpected native read order")
                require((argument(0), argument(1), argument(2)) == (42, READ_BUFFER, SAVE_SIZE), "Unexpected native read arguments")
                data = payload[:SAVE_SIZE]
                if data:
                    uc.mem_write(READ_BUFFER, data)
                calls.append("read")
                cdecl_return(len(data))
            elif address == CLOSE:
                require(calls == ["sprintf", "open", "read"] and argument(0) == 42, "Unexpected native close")
                calls.append("close")
                cdecl_return(0)
            else:
                end = 0x4269BC if entry == METADATA else 0x42691A
                require(entry <= address < end, f"Unexpected native execution at 0x{address:08X}")

        uc.hook_add(UC_HOOK_CODE, hook)
        try:
            uc.emu_start(entry, RETURN + 1, timeout=2_000_000, count=20_000)
        except UcError as error:
            raise ValueError(f"Native emulation failed at 0x{uc.reg_read(UC_X86_REG_EIP):08X}: {error}") from error
        require(bool(stopped), "Native reader exceeded its instruction/time limit")
        expected_calls = ["sprintf", "open"] + (["read", "close"] if present else [])
        require(calls == expected_calls, "Native reader did not complete the expected CRT calls")
        result = {
            "stopped": stopped,
            "eax": uc.reg_read(UC_X86_REG_EAX),
            "read_buffer": bytes(uc.mem_read(READ_BUFFER, SAVE_SIZE)),
            "runtime_buffer": bytes(uc.mem_read(RUNTIME_BUFFER, SAVE_SIZE)),
        }
        if stopped == "returned":
            require(uc.reg_read(UC_X86_REG_ESP) == stack_top + 4, "Native reader unbalanced its cdecl stack")
        return result

    def accepted(self, payload: bytes, slot: int = 2) -> None:
        for entry in (METADATA, LOADER):
            result = self.run(entry, payload, slot=slot)
            require(result["read_buffer"] == payload[:SAVE_SIZE], "Native read buffer differs from input")
            if entry == METADATA:
                require(result["stopped"] == "returned" and result["eax"] == 0, "Native metadata reader rejected save")
                require(result["runtime_buffer"] == SENTINEL, "Metadata reader changed the runtime save buffer")
            else:
                require(result["stopped"] == "restore", "Native loader rejected save before scene restoration")
                require(result["runtime_buffer"] == payload[:SAVE_SIZE], "Native loader did not copy the exact save bytes")

    def rejected(self, payload: bytes, *, present: bool = True) -> None:
        for entry in (METADATA, LOADER):
            result = self.run(entry, payload, present=present)
            require(result["stopped"] == "returned" and result["eax"] == 0xFFFFFFFF, "Native reader accepted invalid/missing data")
            require(result["runtime_buffer"] == SENTINEL, "Rejected save changed the runtime save buffer")

    def controls(self, valid: bytes) -> list[str]:
        cases = {
            "empty": b"",
            "truncated_header": valid[:3],
            "truncated_payload": valid[:-1],
            "header_zero": struct.pack("<I", 0) + valid[4:],
            "header_two": struct.pack("<I", 2) + valid[4:],
        }
        for label, payload in cases.items():
            try:
                self.rejected(payload)
            except ValueError as error:
                raise ValueError(f"Negative control {label}: {error}") from error
        self.rejected(valid, present=False)
        self.accepted(valid + b"trailing bytes")
        return [*cases, "missing_file", "native_accepts_oversized_prefix"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, required=True, help="Original, SHA-256-pinned DC2TY.exe")
    parser.add_argument("--json", action="store_true", help="Write machine-readable evidence to stdout")
    parser.add_argument("saves", type=Path, nargs="+", metavar="SAVE", help="Native .dat exports to verify")
    args = parser.parse_args()
    try:
        native = NativeReaders(args.exe)
        report = {"exe_sha256": EXE_SHA256, "save_size": SAVE_SIZE, "verified": [], "controls": []}
        for index, path in enumerate(args.saves):
            with path.open("rb") as file:
                payload = file.read(SAVE_SIZE + 1)
            require(len(payload) == SAVE_SIZE, f"{path}: expected exactly {SAVE_SIZE} bytes")
            native.accepted(payload)
            report["verified"].append({"path": str(path.resolve()), "sha256": hashlib.sha256(payload).hexdigest(),
                                       "metadata_return": 0, "loader_buffer": "0xAD0A58", "copied_bytes": SAVE_SIZE,
                                       "stopped_before": "0x426580"})
            if index == 0:
                report["controls"] = native.controls(payload)
        report["limitation"] = "Checks native size/header acceptance and byte copying; stops before scene restoration."
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            for item in report["verified"]:
                print(f"PASS {item['path']}: native metadata accepted, loader copied {SAVE_SIZE} exact bytes")
            print(f"PASS {len(report['controls'])} native controls (both reader routines)")
            print(report["limitation"])
        return 0
    except (OSError, ValueError, pefile.PEFormatError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
