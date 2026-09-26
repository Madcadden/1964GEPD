"""Publish the selected, byte-identical executable to the existing v0.2.2 release."""
from __future__ import annotations

import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import urllib.request
import zipfile

REPO = 'Madcadden/1964GEPD'
TAG = 'automatic-mod-compatibility-v0.2.2'
RELEASE_ID = 396012238
OLD_ZIP_HASH = '24003ef4b102ecf53d6565eb94f0c1384a16e295c50d11b6e5b507ecb36e989e'
EXE_HASH = '98273a8b7547dcbace27b7d4e70b5dc420f1b026c78db7bbda20d193212846cf'
INJECTOR_HASH = '870509bb4553b16d679edbae1349948943476bee6ee2c8e5d712a945b4c584cd'
PACKAGE = '1964GEPD-Automatic-Mod-Compatibility-v0.2.2.zip'
SOURCE = '1964GEPD-Automatic-Mod-Compatibility-v0.2.2-Source.zip'
ROOT = Path.cwd()
APPROVED = ROOT / 'approved'
OUT = ROOT / 'release-out'


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError(message)


def git(*args: str) -> str:
    return subprocess.check_output(['git', *args], text=True).strip()


def canonical(data: bytes) -> bytes:
    return data.replace(b'\r\n', b'\n')


def api(path: str, method: str = 'GET', data=None):
    require(path.startswith(f'/repos/{REPO}/'), 'Unexpected API target')
    payload = None if data is None else json.dumps(data).encode()
    req = urllib.request.Request('https://api.github.com' + path, data=payload, method=method,
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                 'Accept': 'application/vnd.github+json', 'Content-Type': 'application/json',
                 'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': '1964GEPD-release-maintenance'})
    with urllib.request.urlopen(req, timeout=60) as response:
        raw = response.read()
    return json.loads(raw) if raw else None


def verify_sources() -> None:
    manifest = json.loads((APPROVED / 'build-manifest.json').read_text(encoding='utf-8-sig'))
    require(sha((APPROVED / '1964.exe').read_bytes()) == EXE_HASH, 'Wrong executable')
    require(manifest['output_sha256'].lower() == EXE_HASH, 'Wrong build manifest')
    for name, expected in manifest['sources_sha256'].items():
        file = ROOT / name
        require(file.is_file(), f'Missing source: {name}')
        raw = file.read_bytes()
        variants = {sha(raw)}
        if file.suffix.lower() in {'.c', '.h', '.rc', '.vcxproj'}:
            lf = canonical(raw)
            variants.update({sha(lf), sha(lf.replace(b'\n', b'\r\n'))})
        require(expected.lower() in variants, f'Compiled source differs: {name}')
    print(f"Verified {len(manifest['sources_sha256'])} corresponding source files")


def prepare() -> None:
    require(os.environ['GITHUB_REF_NAME'] == 'codex/auto-mod-goldeneye-depth-universal',
            'Preparation is restricted to the candidate branch')
    patch = ROOT / 'tools/goldeneye-depth-compat.patch'
    video = ROOT / 'win32/Dll_Video.c'
    compiled = (APPROVED / 'Dll_Video.compiled.c').read_bytes()
    require(sha(compiled) == '028357021feb03c127b0e01f2cca98ff6c0fbc41e3aee692fdb6f793ddb86314',
            'Wrong compiled video source')
    if patch.exists():
        with zipfile.ZipFile(APPROVED / 'source.zip') as source:
            require(canonical(video.read_bytes()) == canonical(source.read('win32/Dll_Video.c')),
                    'Candidate video source moved')
        video.write_bytes(canonical(compiled))
        patch.unlink()
    verify_sources()
    doc = ROOT / 'docs/releases/automatic-mod-compatibility-v0.2.2.md'
    text = doc.read_text(encoding='utf-8')
    text, count = re.subn(r'\*\*SHA-256\*\*.*?(?=\n\[Source\])',
        '**SHA-256**\n\n[Current file checksums](https://github.com/' + REPO +
        '/releases/download/' + TAG + '/SHA256SUMS.txt)\n', text, flags=re.S)
    require(count == 1, 'Expected one checksum section')
    text = text.replace('https://github.com/' + REPO + '/tree/be617351fed4b51c901d698f25eb43f5175121d7',
                        'https://github.com/' + REPO + '/tree/automatic-mod-compatibility')
    doc.write_text(text, encoding='utf-8', newline='\n')
    git('config', 'user.name', 'github-actions[bot]')
    git('config', 'user.email', '41898282+github-actions[bot]@users.noreply.github.com')
    git('add', 'win32/Dll_Video.c', 'tools/goldeneye-depth-compat.patch', str(doc.relative_to(ROOT)))
    if git('diff', '--cached', '--name-only'):
        git('commit', '-m', 'Integrate selected release source')
        git('push', 'origin', 'HEAD:codex/auto-mod-goldeneye-depth-universal')
    print('Prepared source commit:', git('rev-parse', 'HEAD'))


def fetch_public(url: str) -> bytes:
    require(url.startswith('https://github.com/' + REPO + '/releases/download/'),
            'Unexpected release download target')
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def publish() -> None:
    require(os.environ['GITHUB_REF_NAME'] == 'automatic-mod-compatibility',
            'Publication requires the release branch')
    verify_sources()
    require(not (ROOT / 'tools/goldeneye-depth-compat.patch').exists(), 'Unintegrated source')
    OUT.mkdir(exist_ok=True)
    release = api(f'/repos/{REPO}/releases/tags/{TAG}')
    require(release['id'] == RELEASE_ID and not release['immutable'], 'Wrong or immutable release')
    assets = {a['name']: a for a in release['assets']}
    require(PACKAGE in assets and 'SHA256SUMS.txt' in assets, 'Existing release assets missing')
    original = fetch_public(assets[PACKAGE]['browser_download_url'])
    require(sha(original) == OLD_ZIP_HASH, 'Release changed; stop rather than overwrite newer work')
    backup = OUT / 'previous'
    backup.mkdir(exist_ok=True)
    (backup / PACKAGE).write_bytes(original)
    (backup / 'release.json').write_text(json.dumps(release, indent=2), encoding='utf-8')
    (backup / 'SHA256SUMS.txt').write_bytes(fetch_public(assets['SHA256SUMS.txt']['browser_download_url']))
    with zipfile.ZipFile(io.BytesIO(original)) as z:
        contents = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    require(sha(contents['plugin/Mouse_Injector.dll']) == INJECTOR_HASH, 'Injector changed')
    require(all(not n.lower().endswith(('.z64', '.n64', '.v64', '.sav', '.ini')) for n in contents),
            'Unexpected game or settings data')
    commit = git('rev-parse', 'HEAD')
    old_exe_hash = sha(contents['1964.exe'])
    contents['1964.exe'] = (APPROVED / '1964.exe').read_bytes()
    readme = contents['README-v0.2.2.txt'].decode('utf-8-sig')
    readme = re.sub(r'Updated in place on [^\n]+\n\n', '', readme)
    readme = readme.replace('be617351fed4b51c901d698f25eb43f5175121d7', commit)
    contents['README-v0.2.2.txt'] = readme.encode('utf-8')
    contents['LICENSE'] = (ROOT / 'LICENSE').read_bytes()
    sums = f'{EXE_HASH}  1964.exe\n{INJECTOR_HASH}  plugin/Mouse_Injector.dll\n'
    contents['SHA256SUMS.txt'] = sums.encode('ascii')
    with zipfile.ZipFile(OUT / PACKAGE, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, value in sorted(contents.items()):
            zi = zipfile.ZipInfo(name, (2026, 9, 26, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o100644 << 16
            z.writestr(zi, value)
    subprocess.run(['git', 'archive', '--format=zip', '--output=' + str(OUT / SOURCE), 'HEAD'], check=True)
    package_hash = sha((OUT / PACKAGE).read_bytes())
    source_hash = sha((OUT / SOURCE).read_bytes())
    (OUT / 'SHA256SUMS.txt').write_text(f'{package_hash}  {PACKAGE}\n' + sums +
                                     f'{source_hash}  {SOURCE}\n', encoding='ascii')
    body = release['body']
    require(OLD_ZIP_HASH in body and old_exe_hash in body, 'Existing published hashes do not match')
    body = body.replace(OLD_ZIP_HASH, package_hash).replace(old_exe_hash, EXE_HASH)
    body = body.replace('https://github.com/' + REPO + '/tree/be617351fed4b51c901d698f25eb43f5175121d7',
                        'https://github.com/' + REPO + '/tree/' + commit)
    # Stage and verify all uploads before replacing any existing asset names.
    staged = []
    run_id = os.environ['GITHUB_RUN_ID']
    for name in [PACKAGE, SOURCE, 'SHA256SUMS.txt']:
        file = OUT / name
        temp_name = f'pending-{run_id}-{name}'
        raw = file.read_bytes()
        req = urllib.request.Request(
            f'https://uploads.github.com/repos/{REPO}/releases/{RELEASE_ID}/assets?name={temp_name}',
            data=raw, method='POST', headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                'Content-Type': ('application/zip' if name.endswith('.zip') else 'text/plain'),
                'Accept': 'application/vnd.github+json',
                'User-Agent': '1964GEPD-release-maintenance'})
        with urllib.request.urlopen(req, timeout=60) as response:
            new_asset = json.load(response)
        require(new_asset['size'] == len(raw) and new_asset.get('digest') == 'sha256:' + sha(raw),
                'Uploaded asset verification failed')
        staged.append((name, new_asset))
    current = api(f'/repos/{REPO}/releases/{RELEASE_ID}')
    current_assets = {a['name']: a for a in current['assets']}
    require(current_assets[PACKAGE]['id'] == assets[PACKAGE]['id'] and current['body'] == release['body'],
            'Release concurrently changed')
    previous = []
    renamed = []
    try:
        for name, new_asset in staged:
            if name in assets:
                old_asset = assets[name]
                api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}', 'PATCH',
                    {'name': f'previous-{run_id}-{name}'})
                previous.append((name, old_asset))
            api(f'/repos/{REPO}/releases/assets/{new_asset["id"]}', 'PATCH', {'name': name})
            renamed.append((name, new_asset))
        api(f'/repos/{REPO}/releases/{RELEASE_ID}', 'PATCH', {'body': body})
    except Exception:
        for name, new_asset in reversed(renamed):
            api(f'/repos/{REPO}/releases/assets/{new_asset["id"]}', 'PATCH',
                {'name': f'failed-{run_id}-{name}'})
        for name, old_asset in reversed(previous):
            api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}', 'PATCH', {'name': name})
        raise
    for _, old_asset in previous:
        api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}', 'DELETE')
    result = api(f'/repos/{REPO}/releases/{RELEASE_ID}')
    for name in [PACKAGE, SOURCE, 'SHA256SUMS.txt']:
        asset = next(a for a in result['assets'] if a['name'] == name)
        require(asset['digest'] == 'sha256:' + sha((OUT / name).read_bytes()), 'Final asset mismatch')
    require(result['body'] == body and result['tag_name'] == TAG and result['name'] == release['name'],
            'Release metadata mismatch')
    report = {'release': result['html_url'], 'source_commit': commit, 'package_sha256': package_hash,
              'exe_sha256': EXE_HASH, 'injector_sha256': INJECTOR_HASH, 'source_sha256': source_hash}
    (OUT / 'publication.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    require(os.environ.get('GITHUB_REPOSITORY') == REPO, 'Wrong repository')
    require(len(sys.argv) == 2 and sys.argv[1] in {'prepare', 'publish'}, 'Select prepare or publish')
    {'prepare': prepare, 'publish': publish}[sys.argv[1]]()
