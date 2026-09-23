"""Build a manifest-verified preview from a frozen local installation; never install."""
import argparse, base64, hashlib, json, shutil, zipfile
from pathlib import Path

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--game',type=Path,required=True)
    ap.add_argument('--bridge',type=Path,required=True)
    ap.add_argument('--receipt',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--version',required=True)
    ap.add_argument('--source-commit',default='uncommitted-test-build')
    ap.add_argument('--diagnostic',type=Path,help='Validated diagnostic build override; live installation stays unchanged')
    a=ap.parse_args()
    root=Path(__file__).resolve().parents[2]
    out=a.output.resolve()
    if out.exists(): raise SystemExit('Use a new output directory.')
    receipt=json.loads(a.receipt.read_text(encoding='utf-8-sig'))
    for path,key in [(a.game/'amalur_camera.dll','dll'),(a.bridge/'amalur-xr-smoke.exe','bridge')]:
        if sha(path)!=receipt[key].lower():raise SystemExit(f'Installed {key} differs from source receipt')
    original=a.game/'koa.exe.amalur-native-startup-original'
    old,new=original.read_bytes(),(a.game/'koa.exe').read_bytes()
    if sha(original)!='16a400f6e8fc10dbe446a9e57717de9fbe975ef17c004f4ca405e2e4e09cb314':raise SystemExit('Unsupported original game executable')
    out.mkdir(parents=True)
    for name in ['Install.ps1','Launch.ps1','Uninstall.ps1','Common.ps1','Collect-BugReport.ps1']:
        shutil.copy2(Path(__file__).with_name(name),out/name)
    for title,script,args in [('Install','Install.ps1',''),('Install and Launch','Install.ps1',' -Launch'),('Launch VR','Launch.ps1',''),('Uninstall','Uninstall.ps1',''),('Collect Bug Report','Collect-BugReport.ps1','')]:
        (out/(title+'.cmd')).write_text('@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0'+script+'"'+args+'\r\nif errorlevel 1 pause\r\n',encoding='ascii')
    chunks=[];i=0
    while i<len(new):
        if i<len(old) and old[i]==new[i]:i+=1;continue
        start=i;i+=1
        while i<len(new) and (i>=len(old) or old[i]!=new[i]):i+=1
        chunks.append({'offset':start,'data':base64.b64encode(new[start:i]).decode()})
    (out/'game-patch.json').write_text(json.dumps({'newLength':len(new),'chunks':chunks}))
    files=[]
    def add(source,target,preserve=False):
        dest=out/'payload'/target;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
        files.append(dict(target=target,source='payload/'+target,sha256=sha(dest),preserve=preserve))
    def external(source,target,dependency):
        files.append(dict(target=target,name=source.name,sha256=sha(source),dependency=dependency,preserve=False))
    add(a.diagnostic or a.game/'amalur_camera.dll','amalur_camera.dll')
    for name in ['amalur-xr-smoke.exe','amalur-dev-send.exe','amalur-menu-send.exe']:
        add(a.bridge/name,'AmalurVR/'+name)
    add(root/'config/amalur-vr.ini','AmalurVR/amalur-vr.ini',True)
    for name in ['AutoStart.ps1','Launch.ps1','Common.ps1']:
        add(root/'tools/release'/name,'AmalurVR/'+name)
    for name in ['d3dx.ini','d3dxdm.ini','amalur-source.ini']:
        add(a.game/name,name,name=='amalur-source.ini')
    mods=['amalur_dev.lua','amalur_dispatch.json','amalur_dispatch.lua','amalur_menu.json','amalur_menu.lua',
          'amalur_startup.json','amalur_startup.lua','amalur_vr_cinematics.json','amalur_vr_cinematics.lua',
          'amalur_vr_cinematics_entry.lua','amalur_vr_dialogue_gaze.lua','amalur_finisher_icon.json',
          'amalur_finisher_icon.lua','amalur_save_anywhere.json','amalur_save_anywhere.lua']
    for name in mods:add(a.game/'mods'/name,'mods/'+name)
    for name in ['amalur_startup_options.txt']:
        add(a.game/'mods'/name,'mods/'+name,True)
    # Behavior markers only. No dumps, captures, requests, logs, profiles or saves.
    for name in ['amalur-owned-melee.enable','amalur-melee-contact.enable','amalur-melee-effects-probe.enable']:
        if (a.game/name).exists():add(a.game/name,name)
    for name,dep in [('d3d11.dll','geo11'),('nvapi.dll','geo11'),('d3dcompiler_47.dll','geo11'),('d3dcompiler_46.dll','stereo'),('dinput8.dll','framework'),('re_mod.dll','framework')]:
        external(a.game/name,name,dep)
    add(a.bridge/'openxr_loader.dll','AmalurVR/openxr_loader.dll')
    for source in sorted((a.game/'ShaderFixes').iterdir()):
        if source.suffix not in ['.txt','.hlsl','.ini']:continue
        if (root/'shaders/ShaderFixes'/source.name).exists() or source.name=='mouse.hlsl':add(source,'ShaderFixes/'+source.name)
        else:external(source,'ShaderFixes/'+source.name,'stereo')
    dependencies=[
        dict(id='framework',title='Re-Reckoning Mod framework by crtzrms (Nexus mod 9)',url='https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9',reason='Author forbids re-uploading; original download required.'),
        dict(id='geo11',title='geo11 x32 (matching tested build)',url='https://helixmod.blogspot.com/2022/06/announcing-new-geo-11-3d-driver.html',reason='Imported from original distribution; exact file checksums required.'),
        dict(id='stereo',title='Kingdoms of Amalur Re-Reckoning stereo fix by Mike_ar69 (Alpha 0.1)',url='https://helixmod.blogspot.com/2021/01/kingdoms-of-amalur-re-reckoning.html',reason='Downloaded automatically from the original author host; exact archive hash checked.')]
    downloads=[dict(id='geo11',url='https://bo3b.s3.amazonaws.com/geo-11+v0.6.56.zip',name='geo11.zip',sha256='1ce44900b312f6c4848a35292f65bbf31a5fa7e61befa56356d14519091fc182'),dict(id='stereo',url='https://s3.amazonaws.com/Mike_Ar69/KOARR_Mike_ar69.rar',name='stereo.rar',sha256='6663e29452f5cd1b4571ac64bc24d2707a3396f7df4f669179ec48ea5e58bdba')]
    manifest=dict(version=a.version,sourceCommit=a.source_commit,game=dict(originalHash=sha(original),patchedHash=sha(a.game/'koa.exe'),patch='game-patch.json',patchHash=sha(out/'game-patch.json')),files=files,dependencies=dependencies,downloads=downloads,hudCaches=['887f6506d28f9ff1','bd9cebc4f7e1ed36','cc7258d9790a0bbd','bf098be2e4587ca5'])
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (out/'Dependencies').mkdir()
    (out/'Dependencies/Get Mod Framework.url').write_text('[InternetShortcut]\nURL=https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9\n')
    licenses=out/'licenses';licenses.mkdir()
    shutil.copy2(root/'tools/release/licenses/OpenXR-Apache-2.0.txt',licenses/'OpenXR-Apache-2.0.txt')
    for component,name in [('minhook','LICENSE.txt'),('mgs5vr','LICENSE'),('VRScreenCap','LICENSE')]:
        shutil.copy2(root/'third_party'/component/name,licenses/(component+'-LICENSE.txt'))
    (licenses/'NOTICES.md').write_text('OpenXR.Loader 1.0.10.2: Khronos Group, Apache-2.0; https://github.com/KhronosGroup/OpenXR-SDK\nMinHook, MGS5VR and VRScreenCap retain their included license notices.\nGeo11 and the Mike_ar69 stereo fix are fetched from their original hosts, not included in this archive. Re-Reckoning Mod framework by crtzrms must be downloaded from Nexus by the user.\n')
    (out/'DEPENDENCIES.md').write_text('# Original dependency downloads\n\nDownload the Re-Reckoning framework from Nexus and extract it into `Dependencies` (subfolders are fine), then double-click **Install and Launch.cmd**. Geo11 and the stereo shader pack are downloaded automatically from their original hosts. Already installed matching dependencies are reused. Unrecognized versions stop before game changes. Do not install another game\'s rMod or REFramework.\n\n'+''.join(f'- [{d["title"]}]({d["url"]}) — {d["reason"]}\n' for d in dependencies)+'\n`manifest.json` lists every required filename and SHA-256. This preview does not bypass download logins or redistribute restricted dependencies. Windows built-in tar extracts the original archives; if unavailable, extract them manually into Dependencies.\n')
    for name in ['CONTROLS.md']:shutil.copy2(root/name,out/name)
    shutil.copy2(root/'docs/INSTALL.md',out/'README.md')
    # Check that applying only shipped changed bytes exactly reproduces installed EXE.
    reconstructed=bytearray(old);reconstructed.extend(b'\0'*max(0,len(new)-len(old)));del reconstructed[len(new):]
    for c in chunks: b=base64.b64decode(c['data']);reconstructed[c['offset']:c['offset']+len(b)]=b
    assert reconstructed==new
    forbidden={'.sav','.dmp','.pak','.pdb','.obj','.log'}
    assert not any(p.suffix in forbidden or p.name=='koa.exe' for p in out.rglob('*') if p.is_file())
    archive=out.parent/'KingdomsOfAmalurVR.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(out.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(out))
    (out.parent/'KingdomsOfAmalurVR.zip.sha256').write_text(sha(archive)+'  KingdomsOfAmalurVR.zip\n')
    print(json.dumps(dict(zip=str(archive),sha256=sha(archive),bundled=sum(bool(e.get('source')) for e in files),external=sum(not e.get('source') for e in files),patchBytes=sum(len(base64.b64decode(c['data'])) for c in chunks)),indent=2))

if __name__=='__main__':main()
