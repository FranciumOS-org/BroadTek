# SPDX-License-Identifier: MIT
# Loaded by adv_patch_lldb.sh. Addresses are from macOS 26.7.2 (25G332); every
# site's bytes are checked first, and nothing is written unless all of them match.
import lldb

PROLOGUE = bytes.fromhex("8b8f3403000081f9d0070000")
COUNT = bytes.fromhex("4157415641554154534883ec")

# name, unslid address, expected bytes, replacement. Each site is just past
# push %rbp / mov %rsp,%rbp, so "mov $n,%al; pop %rbp; ret" is a clean return.
SITES = [
    ("extended advertising (*0x918)", 0x10033abf0, PROLOGUE, bytes.fromhex("b0015dc3")),
    ("multiple advertising (*0x9c0)", 0x10033b716, PROLOGUE, bytes.fromhex("b0015dc3")),
    ("advertising instances (*0xf88)", 0x100357fa0, COUNT, bytes.fromhex("b0045dc3")),
]


def patch(debugger, command, result, internal_dict):
    process = debugger.GetSelectedTarget().GetProcess()
    target = process.GetTarget()
    header = target.GetModuleAtIndex(0).GetObjectFileHeaderAddress().GetLoadAddress(target)
    slide = header - 0x100000000
    print("bluetoothd pid %d, slide 0x%x" % (process.GetProcessID(), slide))

    error = lldb.SBError()
    for name, addr, want, _ in SITES:
        have = process.ReadMemory(addr + slide, len(want), error)
        if not error.Success() or have != want:
            print("MISMATCH at %s (0x%x): %s. Nothing was changed."
                  % (name, addr, have.hex() if have else error))
            return

    for name, addr, _, new in SITES:
        written = process.WriteMemory(addr + slide, new, error)
        ok = error.Success() and written == len(new)
        print("%s: %s" % (name, "patched" if ok else "FAILED: %s" % error))
