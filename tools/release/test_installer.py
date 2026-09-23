"""Windows-only integration tests: disposable fake game; never launch anything."""
from pathlib import Path
import base64, hashlib, json, shutil, subprocess, tempfile, os

def digest(data):return hashlib.sha256(data).hexdigest()

def run(script,*args,ok=True):
    env=os.environ.copy();env.pop('PSModulePath',None)
    r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(script),*map(str,args)],capture_output=True,text=True,env=env)
    if (r.returncode==0)!=ok:raise AssertionError(r.stdout+'\n'+r.stderr)
    return r

def test():
    with tempfile.TemporaryDirectory(prefix='amalur-installer-') as temp:
        root=Path(temp);package=root/'package';game=root/'game';deps=root/'deps'
        for p in [package,game,deps]:p.mkdir()
        for name in ['Common.ps1','Install.ps1','Uninstall.ps1']:shutil.copy2(Path(__file__).with_name(name),package/name)
        (package/'payload').mkdir();(game/'saves').mkdir()
        (game/'saves/slot.sav').write_bytes(b'KEEP SAVE')
        (game/'koa.exe').write_bytes(b'original executable fixture')
        patched=b'original executable fixture patched'
        patch=json.dumps(dict(newLength=len(patched),chunks=[dict(offset=27,data=base64.b64encode(b' patched').decode())])).encode()
        (package/'game-patch.json').write_bytes(patch)
        (package/'payload/mod.dll').write_bytes(b'mod');(package/'payload/settings.ini').write_bytes(b'defaults')
        (deps/'dep.dll').write_bytes(b'dep')
        m=dict(version='test',game=dict(originalHash=digest((game/'koa.exe').read_bytes()),patchedHash=digest(patched),patch='game-patch.json',patchHash=digest(patch)),downloads=[],dependencies=[dict(id='test',title='fixture',url='https://example.invalid')],hudCaches=[],files=[dict(target='mod.dll',source='payload/mod.dll',sha256=digest(b'mod'),preserve=False),dict(target='AmalurVR/settings.ini',source='payload/settings.ini',sha256=digest(b'defaults'),preserve=True),dict(target='dep.dll',name='dep.dll',dependency='test',sha256=digest(b'dep'),preserve=False)])
        def manifest(): (package/'manifest.json').write_text(json.dumps(m))
        manifest();install=package/'Install.ps1'
        run(install,'-GameDirectory',game,'-Offline',ok=False)
        assert not (game/'mod.dll').exists()
        run(install,'-GameDirectory',game,'-DependencyDirectory',deps,'-CheckOnly','-Offline')
        assert not (game/'.amalur-vr-installer').exists()
        run(install,'-GameDirectory',game,'-DependencyDirectory',deps,'-Offline')
        assert (game/'koa.exe').read_bytes()==patched
        (game/'AmalurVR/settings.ini').write_bytes(b'custom')
        run(install,'-GameDirectory',game,'-Offline')
        assert (game/'AmalurVR/settings.ini').read_bytes()==b'custom'
        run(package/'Uninstall.ps1','-GameDirectory',game)
        assert (game/'koa.exe').read_bytes()==b'original executable fixture'
        assert (game/'saves/slot.sav').read_bytes()==b'KEEP SAVE'
        assert (game/'AmalurVR/settings.ini').read_bytes()==b'custom'
        # An invalid destination after earlier writes must roll the transaction back.
        (game/'blocked').write_bytes(b'not a directory')
        m['files'].append(dict(target='blocked/mod.dll',source='payload/mod.dll',sha256=digest(b'mod'),preserve=False));manifest()
        run(install,'-GameDirectory',game,'-DependencyDirectory',deps,'-Offline',ok=False)
        assert not (game/'mod.dll').exists()
        assert (game/'koa.exe').read_bytes()==b'original executable fixture'
        m['files'][-1]['target']='../escape.dll';manifest()
        run(install,'-GameDirectory',game,'-DependencyDirectory',deps,'-Offline',ok=False)
        assert not (root/'escape.dll').exists()
        m['files'].pop();manifest();(package/'payload/mod.dll').write_bytes(b'tampered')
        run(install,'-GameDirectory',game,'-DependencyDirectory',deps,'-Offline',ok=False)
        assert not (game/'mod.dll').exists()
    print('PASS: dependency preflight, dry-run, fresh install, executable patch, update, settings/saves preservation, uninstall, rollback, traversal and tamper rejection')

if __name__=='__main__':test()
