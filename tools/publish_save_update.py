"""Publish the save-device build to the existing release; hashes-only text change."""
from pathlib import Path
import io, json, os, subprocess, time, urllib.request, zipfile
from publish_v022 import api, sha, fetch_public, require, REPO, TAG, PACKAGE, SOURCE, RELEASE_ID


def verify_public_asset(asset, expected):
    """A reused release filename can temporarily serve its previous cached ZIP."""
    url = asset['browser_download_url']
    require(url.startswith('https://github.com/' + REPO + '/releases/download/'), 'Unexpected public URL')
    observed = []
    for attempt in range(6):
        uncached = url + '?asset_id=' + str(asset['id']) + '&verification=' + os.environ['GITHUB_RUN_ID'] + '-' + str(attempt)
        req = urllib.request.Request(uncached, headers={'Cache-Control': 'no-cache', 'User-Agent': '1964GEPD-release-verification'})
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                digest = sha(response.read())
            observed.append(digest)
            if digest == expected:
                return
        except Exception as error:
            observed.append(type(error).__name__)
        if attempt < 5:
            time.sleep(2 ** attempt)
    raise RuntimeError('Public download mismatch for ' + asset['name'] + ': ' + repr(observed))


require(os.environ.get('GITHUB_REPOSITORY') == REPO and
        os.environ.get('GITHUB_REF_NAME') == 'automatic-mod-compatibility', 'Wrong publication target')
root=Path.cwd(); out=root/'release-out'; out.mkdir(exist_ok=True)
exe=(root/'candidate/1964.exe').read_bytes(); exe_hash=sha(exe)
manifest=json.loads((root/'candidate/build-manifest.json').read_text(encoding='utf-8-sig'))
require(manifest['output_sha256'].lower()==exe_hash, 'Build hash mismatch')
for name, expected in manifest['sources_sha256'].items():
    raw=(root/name).read_bytes(); variants={sha(raw)}
    if Path(name).suffix.lower() in {'.c','.h','.rc','.vcxproj'}:
        lf=raw.replace(b'\r\n',b'\n');variants.update({sha(lf),sha(lf.replace(b'\n',b'\r\n'))})
    require(expected.lower() in variants, 'Compiled source differs: '+name)
release=api(f'/repos/{REPO}/releases/tags/{TAG}')
require(release['id']==RELEASE_ID and not release['immutable'], 'Unexpected release')
assets={a['name']:a for a in release['assets']}
old=fetch_public(assets[PACKAGE]['browser_download_url'])
old_hash=sha(old)
require(old_hash=='ce6e4493e95ad39e8fb7424388153be6f3a56ae26dc10ba92a215edb79f51eb1', 'Release changed; stop')
with zipfile.ZipFile(io.BytesIO(old)) as z:
    files={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
old_exe_hash=sha(files['1964.exe'])
require(old_exe_hash=='69fe0f2a07240983b558a026910807f1cd3d3a115866ef36acb7087180321090','Unexpected previous executable')
injector_hash=sha(files['plugin/Mouse_Injector.dll'])
require(injector_hash=='870509bb4553b16d679edbae1349948943476bee6ee2c8e5d712a945b4c584cd','Injector changed')
backup=out/'previous';backup.mkdir(exist_ok=True)
(backup/PACKAGE).write_bytes(old)
(backup/'release.json').write_text(json.dumps(release,indent=2))
for name in ('SHA256SUMS.txt',SOURCE):
    if name in assets:(backup/name).write_bytes(fetch_public(assets[name]['browser_download_url']))
files['1964.exe']=exe
files['emulator-build-manifest.json']=(root/'candidate/build-manifest.json').read_bytes()
commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
info=files['BUILD-INFO.txt'].decode('utf-8-sig').splitlines()
info=[('Emulator commit: '+commit) if line.startswith('Emulator commit: ') else
      ('Emulator build: https://github.com/'+REPO+'/actions/runs/'+os.environ['GITHUB_RUN_ID']) if line.startswith('Emulator build: ') else line for line in info]
files['BUILD-INFO.txt']=('\n'.join(info)+'\n').encode('utf-8')
files['SHA256SUMS.txt']=(exe_hash+'  1964.exe\n'+injector_hash+'  plugin/Mouse_Injector.dll\n').encode('ascii')
with zipfile.ZipFile(out/PACKAGE,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name,raw in sorted(files.items()):
        info=zipfile.ZipInfo(name,(2026,9,27,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
        z.writestr(info,raw)
with zipfile.ZipFile(out/PACKAGE) as z:
    packaged_manifest=json.loads(z.read('emulator-build-manifest.json').decode('utf-8-sig'))
    require(packaged_manifest['output_sha256'].lower()==sha(z.read('1964.exe')) and
            packaged_manifest['output_bytes']==len(z.read('1964.exe')), 'Packaged build manifest mismatch')
subprocess.run(['git','archive','--format=zip','--output='+str(out/SOURCE),'HEAD'],check=True)
package_hash=sha((out/PACKAGE).read_bytes());source_hash=sha((out/SOURCE).read_bytes())
(out/'SHA256SUMS.txt').write_text(package_hash+'  '+PACKAGE+'\n'+exe_hash+'  1964.exe\n'+injector_hash+'  plugin/Mouse_Injector.dll\n'+source_hash+'  '+SOURCE+'\n',encoding='ascii')
body=release['body']
require(body.count(old_hash)==1 and body.count(old_exe_hash)==1,'Published checksum fields changed')
body=body.replace(old_hash,package_hash).replace(old_exe_hash,exe_hash)
require(body.replace(package_hash,old_hash).replace(exe_hash,old_exe_hash)==release['body'],'Non-hash description change')
run=os.environ['GITHUB_RUN_ID']; staged=[]
for name in (PACKAGE,SOURCE,'SHA256SUMS.txt'):
    raw=(out/name).read_bytes()
    req=urllib.request.Request(f'https://uploads.github.com/repos/{REPO}/releases/{RELEASE_ID}/assets?name=pending-{run}-{name}',data=raw,method='POST',headers={'Authorization':'Bearer '+os.environ['GH_TOKEN'],'Content-Type':'application/zip' if name.endswith('.zip') else 'text/plain','Accept':'application/vnd.github+json','User-Agent':'1964GEPD-release-maintenance'})
    with urllib.request.urlopen(req,timeout=60) as response:new=json.load(response)
    require(new['size']==len(raw) and new.get('digest')=='sha256:'+sha(raw),'Staged upload mismatch')
    staged.append((name,new))
current=api(f'/repos/{REPO}/releases/{RELEASE_ID}')
require(current['body']==release['body'] and {a['name']:a for a in current['assets']}[PACKAGE]['id']==assets[PACKAGE]['id'],'Concurrent release update')
previous=[];renamed=[]
try:
    for name,new in staged:
        if name in assets:
            old_asset=assets[name]
            api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}','PATCH',{'name':f'previous-{run}-{name}'})
            previous.append((name,old_asset))
        api(f'/repos/{REPO}/releases/assets/{new["id"]}','PATCH',{'name':name})
        renamed.append((name,new))
    api(f'/repos/{REPO}/releases/{RELEASE_ID}','PATCH',{'body':body})
    final=api(f'/repos/{REPO}/releases/{RELEASE_ID}')
    require(final['body']==body and final['tag_name']==TAG and final['name']==release['name'],'Final metadata mismatch')
    for name,new in renamed:
        asset=next(a for a in final['assets'] if a['id']==new['id'])
        expected=sha((out/name).read_bytes())
        require(asset['digest']=='sha256:'+expected,'Final server digest mismatch')
        verify_public_asset(asset,expected)
except Exception:
    for name,new in reversed(renamed):
        api(f'/repos/{REPO}/releases/assets/{new["id"]}','PATCH',{'name':f'failed-{run}-{name}'})
    for name,old_asset in reversed(previous):
        api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}','PATCH',{'name':name})
    api(f'/repos/{REPO}/releases/{RELEASE_ID}','PATCH',{'body':release['body']})
    raise
for name,old_asset in previous:api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}','DELETE')
report={'release':release['html_url'],'source_commit':commit,'zip_sha256':package_hash,'exe_sha256':exe_hash,'injector_sha256':injector_hash,'source_sha256':source_hash,'description_hashes_only':True,'public_downloads_verified':True,'packaged_build_manifest_verified':True}
(out/'publication.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
