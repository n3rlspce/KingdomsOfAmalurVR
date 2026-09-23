"""Exercise collection using synthetic logs/saves; no user's saves or uploads."""
import json, os, shutil, subprocess, tempfile, time, zipfile
from pathlib import Path

with tempfile.TemporaryDirectory(prefix='amalur-report-') as tmp:
    root=Path(tmp);game=root/'game';saves=root/'saves';out=root/'reports';package=root/'package'
    for p in (game,saves,package):p.mkdir()
    for name in ('Collect-BugReport.ps1','Common.ps1'):
        shutil.copy2(Path(__file__).with_name(name),package/name)
    (game/'koa.exe').write_bytes(b'fixture')
    (game/'amalur-diagnostic.log').write_text(str(game)+'\n'+os.environ['USERPROFILE']+'\nnormal diagnostic\n')
    for i in range(5):
        p=saves/f'{i}.sav';p.write_bytes(f'fake save {i}'.encode());os.utime(p,(time.time()+i,time.time()+i))
    (saves/'private.txt').write_text('never collect')
    before={p.name:p.read_bytes() for p in saves.iterdir()}
    env=os.environ.copy();env.pop('PSModulePath',None)
    def collect(flag):
        r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(package/'Collect-BugReport.ps1'),
            '-GameDirectory',str(game),'-SaveDirectory',str(saves),'-OutputDirectory',str(out),flag],env=env,capture_output=True,text=True)
        assert r.returncode==0,r.stdout+r.stderr
        return max(out.glob('*.zip'),key=lambda p:p.stat().st_mtime_ns)
    with zipfile.ZipFile(collect('-IncludeSaves')) as z:
        report=json.loads(z.read('report.json').decode('utf-8-sig'))
        assert report['savesIncluded']==3
        assert [z.read(f'save-{i}.sav') for i in range(1,4)]==[f'fake save {i}'.encode() for i in (4,3,2)]
        log=z.read('amalur-diagnostic.log').decode('utf-8-sig')
        assert str(game) not in log and os.environ['USERPROFILE'] not in log
        assert not any('private' in n for n in z.namelist())
    with zipfile.ZipFile(collect('-LogsOnly')) as z:
        assert not any(n.endswith('.sav') for n in z.namelist())
    assert before=={p.name:p.read_bytes() for p in saves.iterdir()}
print('PASS: latest three saves, logs-only, path redaction, excluded files, originals unchanged')
