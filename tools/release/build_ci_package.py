"""Package source-built binaries without a game installation or personal files."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / 'tools/release'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(output, native, bridge, helpers, commit, version):
    output.mkdir(parents=True, exist_ok=False)
    baseline = RELEASE / 'baseline'
    manifest = json.loads((baseline / 'manifest.json').read_text())
    manifest.update(version=version, sourceCommit=commit)
    patch = baseline / 'game-patch.json'
    if sha(patch) != manifest['game']['patchHash']:
        raise ValueError('Baseline game patch hash mismatch')
    shutil.copy2(patch, output / 'game-patch.json')
    targets = set()
    for entry in manifest['files']:
        target = PurePosixPath(entry['target'])
        if target.is_absolute() or '..' in target.parts or ':' in str(target) or '\\' in str(target):
            raise ValueError(f'Unsafe target: {target}')
        if str(target).lower() in targets:
            raise ValueError(f'Duplicate target: {target}')
        targets.add(str(target).lower())
        if not entry.get('source'):
            continue  # Original distributors provide restricted dependencies.
        name = target.name
        if str(target) == 'amalur_camera.dll':
            source = native / 'd3d11.dll'
        elif name in ('amalur-xr-smoke.exe', 'openxr_loader.dll'):
            source = bridge / name
        elif name in ('amalur-dev-send.exe', 'amalur-menu-send.exe'):
            source = helpers / name
        elif name == 'amalur-vr.ini':
            source = ROOT / 'config' / name
        elif target.parts[0] == 'AmalurVR':
            source = RELEASE / name
        elif target.parts[0] == 'ShaderFixes':
            source = ROOT / 'shaders' / target
        elif target.parts[0] == 'mods' and name != 'amalur_startup_options.txt':
            source = ROOT / 'tools' / ('cinematics' if name.startswith('amalur_vr_') else 'developer') / name
        else:
            source = baseline / 'payload' / target
            if sha(source) != entry['sha256']:
                raise ValueError(f'Baseline hash mismatch: {target}')
        entry['source'] = 'payload/' + str(target)
        dest = output / entry['source']
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
        entry['sha256'] = sha(dest)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for name in ('Install', 'Launch', 'Uninstall', 'Common', 'Collect-BugReport'):
        shutil.copy2(RELEASE / (name + '.ps1'), output / (name + '.ps1'))
    commands = [('Install', 'Install', ''), ('Install and Launch', 'Install', ' -Launch'),
                ('Launch VR', 'Launch', ''), ('Uninstall', 'Uninstall', ''),
                ('Collect Bug Report', 'Collect-BugReport', ''), ('Report a Bug', 'Collect-BugReport', ' -OpenForm')]
    for title, script, args in commands:
        ending = 'set "result=%errorlevel%"\necho.\npause\nexit /b %result%\n' if script == 'Install' else 'if errorlevel 1 pause\n'
        (output / (title + '.cmd')).write_text('@echo off\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0' + script + '.ps1"' + args + '\n' + ending, encoding='ascii')
    shutil.copy2(ROOT / 'docs/INSTALL.md', output / 'README.md')
    (output / 'Dependencies').mkdir()
    (output / 'Dependencies/Get Mod Framework.url').write_text('[InternetShortcut]\nURL=https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9\n')
    (output / 'DEPENDENCIES.md').write_text('Download the Nexus framework into `Dependencies`, then run **Install and Launch.cmd**. Other dependencies download from their original hosts.\n\n' + '\n'.join(f'- [{d["title"]}]({d["url"]}) — {d["reason"]}' for d in manifest['dependencies']), encoding='utf-8')
    licenses = output / 'licenses'
    licenses.mkdir()
    shutil.copy2(RELEASE / 'licenses/OpenXR-Apache-2.0.txt', licenses)
    for component, name in [('minhook', 'LICENSE.txt'), ('mgs5vr', 'LICENSE'), ('VRScreenCap', 'LICENSE')]:
        shutil.copy2(ROOT / 'third_party' / component / name, licenses / (component + '-LICENSE.txt'))
    (licenses / 'NOTICES.md').write_text('OpenXR.Loader 1.0.10.2: Khronos Group, Apache-2.0; https://github.com/KhronosGroup/OpenXR-SDK\nMinHook, MGS5VR and VRScreenCap retain their included licenses. Restricted dependencies are not bundled.\n')
    forbidden = {'.sav', '.dmp', '.pak', '.pdb', '.obj', '.log'}
    for path in output.rglob('*'):
        if path.suffix.lower() in forbidden or path.name.lower() == 'koa.exe':
            raise ValueError(f'Forbidden release file: {path}')
    archive = output.parent / 'KingdomsOfAmalurVR.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(output.rglob('*')):
            if path.is_file():
                z.write(path, path.relative_to(output))
    with zipfile.ZipFile(archive) as z:
        if z.testzip():
            raise ValueError('Archive integrity check failed')
        for entry in manifest['files']:
            if entry.get('source') and hashlib.sha256(z.read(entry['source'])).hexdigest() != entry['sha256']:
                raise ValueError('Packaged payload hash mismatch')
    archive.with_suffix('.zip.sha256').write_text(sha(archive) + '  ' + archive.name + '\n')
    print(f'{archive}: {sha(archive)} ({commit})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'native', 'bridge', 'helpers'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--version', required=True)
    build(**vars(parser.parse_args()))
