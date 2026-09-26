"""Publish the selected executable; change only hashes in the release description."""
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
    if not patch.exists():
        verify_sources()
        return
    with zipfile.ZipFile(APPROVED / 'source.zip') as source:
        require(canonical(video.read_bytes()) == canonical(source.read('win32/Dll_Video.c')),
                'Candidate video source moved')
    video.write_bytes(canonical(compiled))
    patch.unlink()
    verify_sources()
    git('config', 'user.name', 'github-actions[bot]')
    git('config', 'user.email', '41898282+github-actions[bot]@users.noreply.github.com')
    git('add', 'win32/Dll_Video.c', 'tools/goldeneye-depth-compat.patch')
    if git('diff', '--cached', '--name-only'):
        git('commit', '-m', 'Integrate selected release source')
        git('push', 'origin', 'HEAD:codex/auto-mod-goldeneye-depth-universal')
    print('Prepared source commit:', git('rev-parse', 'HEAD'))


def fetch_public(url: str) -> bytes:
    require(url.startswith('https://github.com/' + REPO + '/releases/download/'),
            'Unexpected release download target')
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def verify_package(raw: bytes) -> None:
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        require(z.testzip() is None, 'Damaged release ZIP')
        require(sha(z.read('1964.exe')) == EXE_HASH, 'Published executable hash mismatch')
        require(sha(z.read('plugin/Mouse_Injector.dll')) == INJECTOR_HASH,
                'Published injector hash mismatch')
        for line in z.read('SHA256SUMS.txt').decode('ascii').splitlines():
            expected, name = line.split('  ', 1)
            require(sha(z.read(name)) == expected, 'Package checksum mismatch: ' + name)


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
    require('README.txt' in contents and 'BUILD-INFO.txt' in contents, 'Unexpected package layout')
    # Retain installation text; update the provenance that accompanies the binary.
    info = contents['BUILD-INFO.txt'].decode('utf-8-sig')
    info = re.sub(r'^Emulator commit: .*$', 'Emulator commit: ' + commit, info, flags=re.M)
    info = re.sub(r'^Emulator build: .*$',
                  'Emulator build: https://github.com/' + REPO + '/actions/runs/36265443586',
                  info, flags=re.M)
    info = re.sub(r'^Integrity: .*$', 'Integrity: executable and corresponding source hashes checked.',
                  info, flags=re.M)
    contents['BUILD-INFO.txt'] = info.encode('utf-8')
    contents['emulator-build-manifest.json'] = (APPROVED / 'build-manifest.json').read_bytes()
    contents['LICENSE'] = (ROOT / 'LICENSE').read_bytes()
    sums = ''.join(f'{sha(data)}  {name}\n' for name, data in sorted(contents.items())
                   if name != 'SHA256SUMS.txt')
    contents['SHA256SUMS.txt'] = sums.encode('ascii')
    with zipfile.ZipFile(OUT / PACKAGE, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, value in sorted(contents.items()):
            zi = zipfile.ZipInfo(name, (2026, 9, 26, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o100644 << 16
            z.writestr(zi, value)
    verify_package((OUT / PACKAGE).read_bytes())
    subprocess.run(['git', 'archive', '--format=zip', '--output=' + str(OUT / SOURCE), 'HEAD'], check=True)
    package_hash = sha((OUT / PACKAGE).read_bytes())
    source_hash = sha((OUT / SOURCE).read_bytes())
    (OUT / 'SHA256SUMS.txt').write_text(f'{package_hash}  {PACKAGE}\n' + sums +
                                     f'{source_hash}  {SOURCE}\n', encoding='ascii')
    body = release['body']
    require(body.count(OLD_ZIP_HASH) == 1 and body.count(old_exe_hash) == 1 and
            body.count(INJECTOR_HASH) == 1, 'Existing published hashes do not match')
    body = body.replace(OLD_ZIP_HASH, package_hash).replace(old_exe_hash, EXE_HASH)
    require(re.sub(r'[0-9a-f]{64}', '<hash>', body) ==
            re.sub(r'[0-9a-f]{64}', '<hash>', release['body']), 'Non-hash description change')
    # Stage and verify all uploads before replacing any existing asset names.
    staged = []
    run_id = os.environ['GITHUB_RUN_ID']
    for name in [PACKAGE, SOURCE, 'SHA256SUMS.txt']:
        raw = (OUT / name).read_bytes()
        temp_name = f'pending-{run_id}-{name}'
        req = urllib.request.Request(
            f'https://uploads.github.com/repos/{REPO}/releases/{RELEASE_ID}/assets?name={temp_name}',
            data=raw, method='POST', headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                'Content-Type': ('application/zip' if name.endswith('.zip') else 'text/plain'),
                'Accept': 'application/vnd.github+json', 'User-Agent': '1964GEPD-release-maintenance'})
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
        result = api(f'/repos/{REPO}/releases/{RELEASE_ID}')
        for name in [PACKAGE, SOURCE, 'SHA256SUMS.txt']:
            asset = next(a for a in result['assets'] if a['name'] == name)
            downloaded = fetch_public(asset['browser_download_url'] + '?verify=' + run_id)
            require(sha(downloaded) == sha((OUT / name).read_bytes()), 'Published download mismatch')
            require(asset['digest'] == 'sha256:' + sha(downloaded), 'Published digest mismatch')
            if name == PACKAGE:
                verify_package(downloaded)
        require(result['body'] == body and result['tag_name'] == TAG and
                result['name'] == release['name'], 'Release metadata mismatch')
    except Exception:
        for name, new_asset in reversed(renamed):
            api(f'/repos/{REPO}/releases/assets/{new_asset["id"]}', 'PATCH',
                {'name': f'failed-{run_id}-{name}'})
        for name, old_asset in reversed(previous):
            api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}', 'PATCH', {'name': name})
        api(f'/repos/{REPO}/releases/{RELEASE_ID}', 'PATCH', {'body': release['body']})
        raise
    for _, old_asset in previous:
        api(f'/repos/{REPO}/releases/assets/{old_asset["id"]}', 'DELETE')
    report = {'release': result['html_url'], 'source_commit': commit, 'package_sha256': package_hash,
              'exe_sha256': EXE_HASH, 'injector_sha256': INJECTOR_HASH, 'source_sha256': source_hash,
              'published_downloads_rehashed': True, 'description_only_hashes_changed': True}
    (OUT / 'publication.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    require(os.environ.get('GITHUB_REPOSITORY') == REPO, 'Wrong repository')
    require(len(sys.argv) == 2 and sys.argv[1] in {'prepare', 'publish'}, 'Select prepare or publish')
    {'prepare': prepare, 'publish': publish}[sys.argv[1]]()
