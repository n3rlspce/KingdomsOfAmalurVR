"""Stage a revision-checked early splash bypass; never installs or launches."""
from pathlib import Path
import hashlib
import argparse
import json
import struct
import subprocess
from fast_start import Chunk, batch_entries, patch_splash, HASHES

EXE_HASH = '16a400f6e8fc10dbe446a9e57717de9fbe975ef17c004f4ca405e2e4e09cb314'
ARCHIVE_HASH = 'ba34a31a0acb861d7fd1fba118d094e00e5d572e0d3f10df142815750bf44e4c'
INITIAL_HASH = '7776e3847f3acd326962e10ba3c78280265026e28fc19f0b682763f6545015dd'

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def patch_executable(data):
    assert hashlib.sha256(data).hexdigest() == EXE_HASH, 'Unsupported executable'
    # RVA 0x665a6f: cmp eax,3; jb logo_entry. Fallthrough completes the
    # native three-screen sequence, clears its UI flag, and returns normally.
    offset = 0x664e72
    assert data[offset-3:offset+2] == bytes.fromhex('83f803721a')
    assert data[offset+2:offset+28] == bytes.fromhex('c786bc06000000000000c6813401000000c68695060000015ec3')
    result = bytearray(data)
    result[offset:offset+2] = b'\x90\x90'
    assert sum(a != b for a,b in zip(data,result)) == 2
    return result

def patch_splash_batch(data):
    entries = batch_entries(data)
    result = []
    found = 0
    for ident,payload in entries:
        if ident == 1645799:
            assert hashlib.sha256(payload).hexdigest() == HASHES['1645799.lua_bxml']
            chunk = Chunk(payload); assert chunk.bytes() == payload
            patch_splash(chunk)
            payload = chunk.bytes(); found += 1
        result.append((ident,payload))
    assert found == 1
    output = struct.pack('<I',len(result)) + b''.join(struct.pack('<II',i,len(p)) for i,p in result) + b''.join(p for _,p in result)
    after = batch_entries(output)
    assert [(i,p) for i,p in entries if i != 1645799] == [(i,p) for i,p in after if i != 1645799]
    return output

def finish_package(game, out, archive, original_hash, patched_exe):
    """Pack an already extracted, revision-checked source archive."""
    contents = out/'contents'
    unpack = game/'modding/pakfileunpacker.exe'
    with (out/'build.log').open('w') as log:
        batch=contents/'134230570_klua.batch'
        batch.write_bytes(patch_splash_batch(batch.read_bytes()))
        # Cover both asset lookup paths in the selected archive. Which copy
        # the early UI uses still requires a live, no-input startup test.
        loose = contents/'1645799.lua_bxml'
        if loose.exists():
            assert digest(loose) == HASHES[loose.name], 'Unsupported loose splash'
        loose.write_bytes(dict(batch_entries(batch.read_bytes()))[1645799])
        files=out/'files.txt'
        files.write_text('\n'.join(str(p.resolve()) for p in sorted(contents.rglob('*')) if p.is_file())+'\n')
        subprocess.run([str(game/'modding/pakfilebuilder.exe'),'-c',str(files),str(out/archive)],stdout=log,stderr=log,check=True)
        verify=out/'verify';verify.mkdir()
        subprocess.run([str(unpack),str(out/archive),'unpack',str(verify),'134230570_klua.batch','1645799.lua_bxml'],stdout=log,stderr=log,check=True)
    assert (verify/batch.name).read_bytes() == batch.read_bytes()
    assert (verify/loose.name).read_bytes() == loose.read_bytes()
    (out/'koa.exe').write_bytes(patched_exe)
    (out/'manifest.json').write_text(json.dumps({
        'exeOriginal':EXE_HASH, 'exePatched':digest(out/'koa.exe'),
        'archiveName':archive, 'archiveOriginal':original_hash, 'archivePatched':digest(out/archive)},indent=2))
    print('Staged: two native logo-gate bytes; only splash script changed in active batch; main menu byte-identical.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--archive', choices=['patch_0.pak', 'initial_0.pak'], default='patch_0.pak')
    args = parser.parse_args()
    game,out = args.game.resolve(),args.output.resolve()
    archive = args.archive
    original_hash = INITIAL_HASH if archive == 'initial_0.pak' else ARCHIVE_HASH
    assert not out.exists(), 'Use a fresh staging directory'
    assert digest(game/'data'/archive) == original_hash
    patched_exe = patch_executable((game/'koa.exe').read_bytes())
    out.mkdir(parents=True); contents=out/'contents'; contents.mkdir()
    unpack=game/'modding/pakfileunpacker.exe'
    with (out/'unpack.log').open('w') as log:
        subprocess.run([str(unpack),str(game/'data'/archive),'unpack',str(contents)],stdout=log,stderr=log,check=True)
    finish_package(game, out, archive, original_hash, patched_exe)

if __name__ == '__main__': main()
