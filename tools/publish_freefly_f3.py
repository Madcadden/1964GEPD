"""Publish only the confirmed injector to the current emulator release."""
from __future__ import annotations
from pathlib import Path
import hashlib
import io
import json
import os
import re
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile

REPO = 'Madcadden/1964GEPD'
TAG = 'automatic-mod-compatibility-v0.2.2'
RID = 396012238
PACKAGE = '1964GEPD-Automatic-Mod-Compatibility-v0.2.2.zip'
EMU_SOURCE = '1964GEPD-Automatic-Mod-Compatibility-v0.2.2-Source.zip'
INPUT_SOURCE = 'Mouse-Injector-FreeFly-F3-Source.zip'
OLD_ZIP = 'f86457d4dfe274e0082de8e60db00d98228676720e32a4cf57cb221afa1ccf10'
EXE = '34ab4aa5dc5ded7a9e04f6950f464e43d6d26b44fa7758e3307406d663fffb32'
OLD_DLL = '870509bb4553b16d679edbae1349948943476bee6ee2c8e5d712a945b4c584cd'
DLL = '9a6db746f29a069d452d60eeb16e399fa9e43a026781e445ac4c089af009826b'
INPUT_COMMIT = '8cdff29fa62f9aca63360c9c60ae380111c74b29'


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def api(path: str, method: str = 'GET', data=None):
    require(path.startswith('/repos/' + REPO + '/'), 'Wrong API target')
    req = urllib.request.Request('https://api.github.com' + path,
        data=None if data is None else json.dumps(data).encode(), method=method,
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                 'Accept': 'application/vnd.github+json', 'Content-Type': 'application/json',
                 'User-Agent': '1964GEPD-release-maintenance'})
    with urllib.request.urlopen(req, timeout=60) as response:
        raw = response.read()
    return json.loads(raw) if raw else None


def download(asset: dict, expected: str) -> bytes:
    url = asset['browser_download_url']
    require(url.startswith('https://github.com/' + REPO + '/releases/download/' + TAG + '/'),
            'Unexpected download target')
    observed = []
    for delay in (0, 1, 2, 4, 8, 16):
        time.sleep(delay)
        query = urllib.parse.urlencode({'verify': expected, 'asset': asset['id'], 'nonce': time.time_ns()})
        req = urllib.request.Request(url + '?' + query, headers={
            'Cache-Control': 'no-cache', 'User-Agent': '1964GEPD-release-verification'})
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                raw = response.read()
            observed.append(sha(raw))
            if sha(raw) == expected:
                return raw
        except urllib.error.HTTPError as error:
            if error.code not in (404, 429, 502, 503, 504):
                raise
            observed.append('HTTP ' + str(error.code))
    raise RuntimeError('Public download did not match: ' + asset['name'] + ': ' + repr(observed))


def archive(path: Path, files: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for name, raw in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 9, 27, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            output.writestr(info, raw)


def readzip(raw: bytes) -> dict[str, bytes]:
    with zipfile.ZipFile(io.BytesIO(raw)) as source:
        require(source.testzip() is None, 'Corrupt ZIP')
        files = {name: source.read(name) for name in source.namelist() if not name.endswith('/')}
        require(len(files) == sum(not n.endswith('/') for n in source.namelist()), 'Duplicate ZIP path')
        require(all(not Path(n).is_absolute() and '..' not in Path(n).parts for n in files), 'Unsafe path')
    return files


def prepare(root: Path, old: bytes, emu_source: bytes) -> tuple[dict[str, bytes], dict]:
    approved = root / 'approved'
    source_files = readzip((approved / 'source.zip').read_bytes())
    manifest_raw = (approved / 'build-manifest.json').read_bytes()
    manifest = json.loads(manifest_raw)
    dll = (approved / 'Mouse_Injector.dll').read_bytes()
    require(sha(dll) == DLL and manifest['Mouse_Injector.dll'] == DLL, 'Not the confirmed F3 DLL')
    require((approved / 'commit.txt').read_text().strip() == 'e43be0d5c130b15980e07f6076fbca2250fbeece',
            'Wrong source artifact')
    for name, raw in source_files.items():
        require(sha(raw) == manifest[name], 'Artifact source mismatch: ' + name)
        require(not name.lower().endswith(('.sav', '.sav1', '.z64', '.n64', '.v64', '.ini', '.dmp')),
                'Unexpected private/game data')
        if Path(name).suffix.lower() in ('.c', '.h', '.rc', '.lib'):
            prepared = (root / 'injector' / name).read_bytes()
            require(prepared.replace(b'\r\n', b'\n') == raw.replace(b'\r\n', b'\n'),
                    'Repository source differs from confirmed build: ' + name)
    native = {p.name: p.read_bytes() for p in (root / 'native').glob('*.txt')}
    require(any(b'F3_PASS' in raw for raw in native.values()), 'Missing native F3 verification')
    require(any(b'F2_REPRODUCED' in raw for raw in native.values()), 'Missing baseline reproduction')
    public_source = dict(source_files)
    for name in ('build-manifest.json', 'compiler.txt', 'build.log', 'pe-info.txt', 'commit.txt'):
        public_source['verification/' + name] = (approved / name).read_bytes()
    for name, raw in native.items():
        public_source['verification/native/' + name] = raw
    public_source['BUILD-F3.md'] = (
        '# Corresponding source for the confirmed FreeFly-F3 DLL\n\n'
        'All patches are already applied in this archive. Do not reapply the patch files.\n'
        'Build with the existing makefile and an i686 MinGW compiler:\n\n'
        '```sh\nmkdir -p obj\nmake CC=i686-w64-mingw32-gcc WINDRES=i686-w64-mingw32-windres\n```\n\n'
        'The original toolchain, source hashes and native test reports are in verification/.\n'
        'Publication uses the unchanged user-tested DLL: `' + DLL + '`.\n'
        'Repository source: `Madcadden/mouse-injector@' + INPUT_COMMIT + '`.\n'
        'User confirmed Free Fly mouse look; this does not certify every ROM or menu.\n'
    ).encode()
    out = root / 'release-out'
    out.mkdir(exist_ok=True)
    archive(out / INPUT_SOURCE, public_source)
    require(sha(old) == OLD_ZIP, 'Current package changed; stop')
    previous = readzip(old)
    require(sha(previous['1964.exe']) == EXE, 'Current emulator changed')
    require(sha(previous['plugin/Mouse_Injector.dll']) == OLD_DLL, 'Current injector changed')
    files = dict(previous)
    files['plugin/Mouse_Injector.dll'] = dll
    files['injector-build-manifest.json'] = manifest_raw
    info = files['BUILD-INFO.txt'].decode('utf-8-sig')
    info = re.sub(r'^Injector [^\n]*\n', '', info, flags=re.M)
    info = info.replace('Gameplay and regression tests: not run.\n', '')
    info += ('Injector repository commit: ' + INPUT_COMMIT + '\n'
        'Injector compiled baseline: e43be0d5c130b15980e07f6076fbca2250fbeece with F3 patches\n'
        'Injector build: https://github.com/Madcadden/1964GEPD/actions/runs/36282441081\n'
        'Injector bytes: ' + str(len(dll)) + '\n'
        'Injector SHA-256: ' + DLL + '\n'
        'Injector toolchain: ' + (approved / 'compiler.txt').read_text().splitlines()[0] + '\n'
        'Injector validation: native Windows regression and DLL-load checks; user confirmed Free Fly.\n')
    files['BUILD-INFO.txt'] = info.encode()
    files['SHA256SUMS.txt'] = (EXE + '  1964.exe\n' + DLL + '  plugin/Mouse_Injector.dll\n').encode('ascii')
    allowed = {'plugin/Mouse_Injector.dll', 'injector-build-manifest.json', 'BUILD-INFO.txt', 'SHA256SUMS.txt'}
    require(files.keys() == previous.keys(), 'Unexpected package entries')
    require(all(files[n] == raw for n, raw in previous.items() if n not in allowed), 'Unrelated file changed')
    archive(out / PACKAGE, files)
    payload = {PACKAGE: (out / PACKAGE).read_bytes(), INPUT_SOURCE: (out / INPUT_SOURCE).read_bytes()}
    sums = (sha(payload[PACKAGE]) + '  ' + PACKAGE + '\n' + files['SHA256SUMS.txt'].decode() +
            sha(emu_source) + '  ' + EMU_SOURCE + '\n' + sha(payload[INPUT_SOURCE]) + '  ' + INPUT_SOURCE + '\n')
    payload['SHA256SUMS.txt'] = sums.encode('ascii')
    (out / 'SHA256SUMS.txt').write_bytes(payload['SHA256SUMS.txt'])
    return payload, {'package_sha256': sha(payload[PACKAGE]), 'emulator_sha256': EXE,
        'injector_sha256': DLL, 'injector_source_sha256': sha(payload[INPUT_SOURCE]),
        'injector_repository_commit': INPUT_COMMIT, 'emulator_unchanged': True}


def publish() -> None:
    require(os.environ.get('GITHUB_REPOSITORY') == REPO and
            os.environ.get('GITHUB_REF_NAME') == 'automatic-mod-compatibility', 'Wrong publication target')
    root = Path.cwd()
    require(subprocess.check_output(['git', '-C', 'injector', 'rev-parse', 'HEAD'], text=True).strip()
            == INPUT_COMMIT, 'Unexpected injector checkout')
    release = api(f'/repos/{REPO}/releases/{RID}')
    require(release['tag_name'] == TAG and not release['immutable'] and not release['draft'], 'Wrong release')
    assets = {a['name']: a for a in release['assets']}
    require(INPUT_SOURCE not in assets, 'Input source asset already exists; review rather than overwrite')
    old = download(assets[PACKAGE], OLD_ZIP)
    emu_source = download(assets[EMU_SOURCE], '67a0441b14c5fe4e949cb2984a6232fad3a3fb4c04d3765cc3055b8d4d3c431f')
    payloads, report = prepare(root, old, emu_source)
    backup = root / 'release-out/previous'
    backup.mkdir(exist_ok=True)
    (backup / PACKAGE).write_bytes(old)
    (backup / 'release.json').write_text(json.dumps(release, indent=2))
    (backup / 'SHA256SUMS.txt').write_bytes(download(assets['SHA256SUMS.txt'], assets['SHA256SUMS.txt']['digest'][7:]))
    body = release['body']
    require(body.count(OLD_ZIP) == 1 and body.count(OLD_DLL) == 1 and body.count(EXE) == 1,
            'Release hash fields changed')
    new_body = body.replace(OLD_ZIP, report['package_sha256']).replace(OLD_DLL, DLL)
    require(new_body.replace(report['package_sha256'], OLD_ZIP).replace(DLL, OLD_DLL) == body,
            'Non-hash description change')
    run = os.environ['GITHUB_RUN_ID']
    staged = []
    for name, raw in payloads.items():
        query = urllib.parse.urlencode({'name': 'pending-' + run + '-' + name})
        req = urllib.request.Request(f'https://uploads.github.com/repos/{REPO}/releases/{RID}/assets?' + query,
            data=raw, method='POST', headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                'Content-Type': 'application/zip' if name.endswith('.zip') else 'text/plain',
                'User-Agent': '1964GEPD-release-maintenance'})
        with urllib.request.urlopen(req, timeout=60) as response:
            asset = json.load(response)
        require(asset['size'] == len(raw) and asset.get('digest') == 'sha256:' + sha(raw), 'Upload mismatch')
        download(asset, sha(raw))
        staged.append((name, asset))
    fresh = api(f'/repos/{REPO}/releases/{RID}')
    require(fresh['body'] == body and all(
        next(a['id'] for a in fresh['assets'] if a['name'] == n) == assets[n]['id']
        for n in (PACKAGE, EMU_SOURCE, 'SHA256SUMS.txt')), 'Concurrent release update')
    moved_old, moved_new = [], []
    try:
        for name, asset in staged:
            if name in assets:
                previous = assets[name]
                api(f'/repos/{REPO}/releases/assets/{previous["id"]}', 'PATCH', {'name': 'previous-' + run + '-' + name})
                moved_old.append((name, previous))
            api(f'/repos/{REPO}/releases/assets/{asset["id"]}', 'PATCH', {'name': name})
            moved_new.append((name, asset))
        api(f'/repos/{REPO}/releases/{RID}', 'PATCH', {'body': new_body})
        final = api(f'/repos/{REPO}/releases/{RID}')
        require(final['body'] == new_body and all(final[k] == release[k]
            for k in ('tag_name', 'name', 'draft', 'prerelease', 'target_commitish')), 'Release metadata changed')
        for name, raw in payloads.items():
            asset = next(a for a in final['assets'] if a['name'] == name)
            require(asset.get('digest') == 'sha256:' + sha(raw), 'Final digest mismatch')
            download(asset, sha(raw))
    except Exception:
        for name, asset in reversed(moved_new):
            api(f'/repos/{REPO}/releases/assets/{asset["id"]}', 'PATCH', {'name': 'failed-' + run + '-' + name})
        for name, asset in reversed(moved_old):
            api(f'/repos/{REPO}/releases/assets/{asset["id"]}', 'PATCH', {'name': name})
        current = api(f'/repos/{REPO}/releases/{RID}')
        if current['body'] == new_body:
            api(f'/repos/{REPO}/releases/{RID}', 'PATCH', {'body': body})
        raise
    for _, asset in moved_old:
        api(f'/repos/{REPO}/releases/assets/{asset["id"]}', 'DELETE')
    report.update(release=final['html_url'], description_changes='hashes only', public_downloads_verified=True)
    (root / 'release-out/publication.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    publish()
