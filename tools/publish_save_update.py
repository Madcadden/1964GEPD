"""Publish the save-device build to the existing release; hashes-only text change."""
from pathlib import Path
import io, json, os, subprocess, time, urllib.request, zipfile
from publish_v022 import api, sha, fetch_public, require, REPO, TAG, PACKAGE, SOURCE, RELEASE_ID


def download_verified(asset, expected):
    """A renamed asset can temporarily inherit the prior URL's cached redirect."""
    require(asset.get('digest') == 'sha256:' + expected, 'Asset digest mismatch')
    last = None
    for attempt in range(5):
        url = asset['browser_download_url'] + '?asset=' + str(asset['id']) + '&verify=' + os.environ['GITHUB_RUN_ID'] + '-' + str(attempt)
        try:
            raw = fetch_public(url)
            last = sha(raw)
            if last == expected:
                return raw
        except (OSError, TimeoutError) as error:
            last = str(error)
        time.sleep(1 + attempt)
    raise RuntimeError('Public download verification failed for ' + asset['name'] + ': ' + str(last))


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
old=download_verified(assets[PACKAGE], 'dd8615e1b5fc9c28c68f27938b3d422511246dd1fa47f50224484a709bdb480e')
old_hash=sha(old)
require(old_hash=='dd8615e1b5fc9c28c68f27938b3d422511246dd1fa47f50224484a709bdb480e', 'Release changed; stop')
with zipfile.ZipFile(io.BytesIO(old)) as z:
    files={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
old_exe_hash=sha(files['1964.exe'])
require(old_exe_hash=='98273a8b7547dcbace27b7d4e70b5dc420f1b026c78db7bbda20d193212846cf','Unexpected previous executable')
require(exe_hash!=old_exe_hash,'Executable unchanged')
injector_hash=sha(files['plugin/Mouse_Injector.dll'])
require(injector_hash=='870509bb4553b16d679edbae1349948943476bee6ee2c8e5d712a945b4c584cd','Injector changed')
backup=out/'previous';backup.mkdir(exist_ok=True)
(backup/PACKAGE).write_bytes(old)
(backup/'release.json').write_text(json.dumps(release,indent=2))
for name in ('SHA256SUMS.txt',SOURCE):
    if name in assets:
        (backup/name).write_bytes(download_verified(assets[name], assets[name]['digest'].removeprefix('sha256:')))
files['1964.exe']=exe
files['SHA256SUMS.txt']=(exe_hash+'  1964.exe\n'+injector_hash+'  plugin/Mouse_Injector.dll\n').encode('ascii')
with zipfile.ZipFile(out/PACKAGE,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name,raw in sorted(files.items()):
        info=zipfile.ZipInfo(name,(2026,9,27,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
        z.writestr(info,raw)
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
        # Download the actual public bytes as well as checking server metadata.
        asset=next(a for a in final['assets'] if a['id']==new['id'])
        download_verified(asset, sha((out/name).read_bytes()))
except Exception:
    for name,new in reversed(renamed):
        api(f'/repos/{REPO}/releases/assets/{new["id"]}','PATCH',{'name':f'failed-{run}-{name}'})
    for name,old_asset in reversed(previous):
        api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}','PATCH',{'name':name})
    api(f'/repos/{REPO}/releases/{RELEASE_ID}','PATCH',{'body':release['body']})
    raise
for name,old_asset in previous:api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}','DELETE')
# Remove only the temporary artifacts left by this update's prior rolled-back run.
for name in (PACKAGE, SOURCE, 'SHA256SUMS.txt'):
    stale = assets.get('failed-36278834177-' + name)
    if stale:api(f'/repos/{REPO}/releases/assets/{stale["id"]}', 'DELETE')
report={'release':release['html_url'],'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'zip_sha256':package_hash,'exe_sha256':exe_hash,'injector_sha256':injector_hash,'source_sha256':source_hash,'description_hashes_only':True,'public_downloads_verified':True}
(out/'publication.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
