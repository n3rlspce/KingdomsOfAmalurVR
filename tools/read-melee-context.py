"""Bounded, read-only melee lifetime capture for Re-Reckoning build 10619381.

No injected code, process writes, input or game launches. Supply a freshly
observed PID/module base. Outputs local JSONL only when context state changes.
"""
import argparse
import ctypes as c
import hashlib
import json
import struct
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("base", type=lambda s: int(s, 0))
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=float, default=45)
    parser.add_argument("--label", default="", help="User-supplied weapon name/type; recorded verbatim, not inferred")
    args = parser.parse_args()
    if not 0 < args.seconds <= 60:
        parser.error("capture duration must be 0 < seconds <= 60")
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [c.c_uint, c.c_int, c.c_uint]
    kernel.OpenProcess.restype = c.c_void_p
    kernel.ReadProcessMemory.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    kernel.CloseHandle.argtypes = [c.c_void_p]
    handle = kernel.OpenProcess(0x1010, False, args.pid)  # QUERY_LIMITED_INFORMATION | VM_READ
    if not handle:
        raise c.WinError(c.get_last_error())

    def read(address, size):
        if address < 0x10000 or address + size > 0x100000000:
            raise ValueError("invalid x86 address")
        data, got = c.create_string_buffer(size), c.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, data, size, c.byref(got)) or got.value != size:
            raise OSError("unreadable process memory")
        return data.raw

    def u(address):
        return struct.unpack("<I", read(address, 4))[0]

    def bounded(value, maximum):
        if value > maximum:
            raise ValueError("unexpected native array count")
        return value

    def resident(manager, index):
        if index < 2 or index >= 1000000:
            return 0
        flags = read(u(manager + 0x28) + index, 1)[0]
        return u(u(manager + 0x18) + index * 4) if flags & 4 and not flags & 16 else 0

    def type_name(address):
        vtable = u(address)
        if not args.base <= vtable < args.base + 0x2000000:
            return None
        descriptor = u(u(vtable - 4) + 12)
        raw = read(descriptor + 8, 128).split(b"\0", 1)[0]
        return raw.decode("ascii") if all(32 <= byte < 127 for byte in raw) else None

    def script_metadata(index):
        # 6b1724 uses the Lua manager, not the Talent manager. Resource indices
        # are manager-local; the same number in the Talent table is unrelated.
        try:
            assets = u(args.base + 0x15f4d34)
            address = resident(assets, index)
            if not address:
                return {"asset": index, "resident": False}
            kind = type_name(address)
            if kind != ".?AVLuaScript@BHG@@":
                raise ValueError("unexpected script resource type")
            pointer, size = u(address + 0x20), bounded(u(address + 0x24), 16000000)
            data = read(pointer, size)
            if not data.startswith(b"\x1bLua"):
                raise ValueError("unexpected script bytecode header")
            name_record = u(address + 0x14)
            name_size = bounded(u(name_record + 4), 1024)
            name = read(u(name_record), name_size).decode("utf-8")
            if (resident(assets, index) != address or u(address + 0x20) != pointer
                    or u(address + 0x24) != size):
                raise ValueError("script resource changed during snapshot")
            return {"asset": index, "resident": True, "pointer": address,
                    "type": kind, "module": name, "bytecodeSize": size,
                    "bytecodeSha256": hashlib.sha256(data).hexdigest()}
        except (OSError, ValueError, UnicodeError) as error:
            return {"asset": index, "unavailable": str(error)}

    def snapshot():
        # 9bac90 -> 76e220: player index zero in the global player list.
        player_manager = u(args.base + 0x15fd5e4)
        players, count = u(player_manager + 0x1650), bounded(u(player_manager + 0x1654), 16)
        player = 0
        for i in range(count):
            candidate = u(players + i * 4)
            if u(candidate + 0x100) == 0:
                if u(candidate) not in (args.base + 0x1359f14, args.base + 0x1359e94):
                    raise ValueError("unexpected player type")
                player = candidate
                break
        if not player:
            return {"state": "no-player"}
        owner = u(player + 0x1ec)
        manager = u(args.base + 0x15fec38)
        pool, slot = manager + 0x2238, owner & 65535
        if not owner & 0x0fff0000 or slot >= bounded(u(pool + 0x20), 65536):
            raise ValueError("invalid owner handle")
        if u(u(pool + 0x1c) + slot * 4) != owner:
            raise ValueError("expired owner generation")
        entity = u(u(pool + 0xc) + slot * 4)
        if u(entity + 0x38) != owner or not u(entity + 0x10c) & 1:
            raise ValueError("inactive owner")

        def part(index):
            address = u(entity + 0x3c + index * 4)
            if u(address + 0x18) != owner or u(address + 0x1c) != index or not u(address + 0x20) & 1:
                raise ValueError("invalid component")
            return address

        talents, physics = part(18), part(15)
        # Verified physics attack-window layout: 0x34-byte entries. These are
        # observations only; an index/key is never retained for native use.
        attack_table = u(physics + 0x2e0)
        attack_count = bounded(u(physics + 0x2e4), 32)

        def attack_entries():
            data = read(attack_table, attack_count * 0x34) if attack_count else b""
            return [{"eventId": struct.unpack_from("<I", data, i * 0x34)[0],
                     "flags": struct.unpack_from("<I", data, i * 0x34 + 0x14)[0],
                     "runtimeIndex": struct.unpack_from("<I", data, i * 0x34 + 0x2c)[0],
                     "key": struct.unpack_from("<I", data, i * 0x34 + 0x30)[0]}
                    for i in range(attack_count)]

        native_attacks = attack_entries()
        keys, count, records = u(talents + 0x24), bounded(u(talents + 0x28), 4096), u(talents + 0x34)
        table, total = u(manager + 0xd0), bounded(u(manager + 0xd4), 1048576)
        assets = u(args.base + 0x15f4dfc)
        # Observe residency even BEFORE an attack creates any talent entries.
        # This distinguishes equip-time loading from first-attack loading.
        dagger_definition = resident(assets, 199)
        dagger_source = {"asset": 199, "resident": bool(dagger_definition)}
        if dagger_definition:
            dagger_source["pointer"] = dagger_definition
            dagger_source["script"] = script_metadata(u(dagger_definition + 0x94))
        key_data = read(keys, count * 4) if count else b""

        def runtime(index):
            if not 0 < index < total:
                return {"index": index, "valid": False}
            address = u(table + index * 4)
            if not address:
                return {"index": index, "valid": False}
            data = read(address, 0x54)
            words = struct.unpack("<21I", data)
            asset = words[1]
            definition = resident(assets, asset)
            details = None
            if definition:
                # Store data for later static interpretation; never call a getter.
                details = {"pointer": definition, "vtable": u(definition),
                           "fields": {hex(offset): u(definition + offset) for offset in
                                      (0xc, 0x94, 0x1a0, 0x1a8, 0x1bc, 0x1f8, 0x1fc, 0x200, 0x208, 0x20c)}}
                if details["fields"]["0x94"] >= 2:
                    details["script"] = script_metadata(details["fields"]["0x94"])
            return {"index": index, "pointer": address, "asset": asset,
                    "active": bool(words[7] & 1), "selfIndex": words[8],
                    "owner": words[9], "target": words[10], "definition": details}

        entries = []
        for i in range(count):
            record = records + i * 24
            base_index, asset, secondary, secondary_count = struct.unpack("<4I", read(record, 16))
            bounded(secondary_count, 256)
            indices = [base_index] + ([u(secondary + j * 4) for j in range(secondary_count)])
            entries.append({"key": struct.unpack_from("<I", key_data, i * 4)[0],
                            "recordAsset": asset, "runtimes": [runtime(index) for index in indices]})
        # External observation is not atomic. Discard samples spanning a table change.
        if (u(talents + 0x28) != count or u(talents + 0x24) != keys or u(talents + 0x34) != records
                or (read(keys, count * 4) if count else b"") != key_data
                or u(manager + 0xd0) != table or u(manager + 0xd4) != total
                or u(player + 0x1ec) != owner
                or part(15) != physics or part(18) != talents
                or u(physics + 0x2e0) != attack_table or u(physics + 0x2e4) != attack_count
                or attack_entries() != native_attacks
                or u(physics + 0x2e0) != attack_table or u(physics + 0x2e4) != attack_count):
            raise ValueError("context changed during snapshot")
        if resident(assets, 199) != dagger_definition:
            raise ValueError("dagger source changed during snapshot")
        return {"state": "sample", "owner": owner, "nativeWindows": attack_count,
                "nativeAttackEntries": native_attacks,
                "daggerSource": dagger_source, "entries": entries}

    try:
        # These native retirement entrypoints remain unhooked by owned melee.
        # b80020 is deliberately detoured by our lifetime observer, so checking
        # its original prologue incorrectly rejects the enabled combat build.
        for rva, signature in ((0xbe1eb0, "8b41048b4c2404"),
                               (0xbbfcb0, "8b51185633c057")):
            expected = bytes.fromhex(signature)
            if read(args.base + rva, len(expected)) != expected:
                raise ValueError(f"native retirement signature mismatch at {rva:x}")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        started, previous, samples, changes = time.monotonic(), None, 0, 0
        with args.output.open("x", encoding="utf-8") as output:
            while time.monotonic() - started < args.seconds:
                try:
                    value = snapshot()
                except (ValueError, OSError) as error:
                    value = {"state": "unavailable", "reason": str(error)}
                value["label"] = args.label
                samples += 1
                encoded = json.dumps(value, sort_keys=True)
                if encoded != previous:
                    output.write(json.dumps({"seconds": round(time.monotonic() - started, 3), **value}) + "\n")
                    output.flush()
                    previous, changes = encoded, changes + 1
                time.sleep(0.05)
        print(json.dumps({"samples": samples, "changes": changes, "output": str(args.output)}))
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    main()
