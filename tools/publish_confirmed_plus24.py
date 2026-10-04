"""Publish the user-confirmed Plus 2.4 DLL without rebuilding or moving tags."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import os
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile

DLL_SHA = 'bfd2f95ed75d1683ab6b4f16e7f2f1f7f0869ed703f061c6870842293080d0a0'
OLD_DLL_SHA = '4ff94a049c7d4d3e5c4bee54e408c7646ddde1a52fff408e06378aa65c2d7eb3'
EXE_SHA = '34ab4aa5dc5ded7a9e04f6950f464e43d6d26b44fa7758e3307406d663fffb32'
INPUT_SOURCE = 'Mouse-Injector-FreeFly-F3-Source.zip'
EMU_SOURCE = '1964GEPD-Automatic-Mod-Compatibility-v0.2.2-Source.zip'


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def api(config, suffix, method='GET', data=None):
    require(suffix.startswith('/'), 'Invalid API suffix')
    request = urllib.request.Request('https://api.github.com/repos/' + config['repo'] + suffix,
        data=None if data is None else json.dumps(data).encode(), method=method,
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                 'Accept': 'application/vnd.github+json', 'Content-Type': 'application/json',
                 'User-Agent': 'Mouse-Injector-release-maintenance'})
    with urllib.request.urlopen(request, timeout=60) as response:
        raw = response.read()
    return json.loads(raw) if raw else None


def download(config, asset, expected):
    prefix = 'https://github.com/' + config['repo'] + '/releases/download/' + config['before']['tag_name'] + '/'
    require(asset['browser_download_url'].startswith(prefix), 'Unexpected asset URL')
    observed = []
    for delay in (0, 1, 2, 4, 8, 16):
        if delay:
            time.sleep(delay)
        query = urllib.parse.urlencode({'asset': asset['id'], 'sha256': expected, 'nonce': time.time_ns()})
        request = urllib.request.Request(asset['browser_download_url'] + '?' + query,
            headers={'Cache-Control': 'no-cache', 'User-Agent': 'Mouse-Injector-release-verification'})
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                raw = response.read()
            observed.append(sha(raw))
            if sha(raw) == expected:
                return raw
        except urllib.error.HTTPError as error:
            if error.code not in (404, 429, 502, 503, 504):
                raise
            observed.append(str(error.code))
    raise RuntimeError('Download mismatch: ' + asset['name'] + ': ' + repr(observed))


def zipfiles(raw):
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        require(archive.testzip() is None, 'Corrupt ZIP')
        names = archive.namelist()
        require(len(names) == len(set(names)), 'Duplicate ZIP entries')
        require(all(not Path(n).is_absolute() and '..' not in Path(n).parts for n in names), 'Unsafe ZIP path')
        return {n: archive.read(n) for n in names}


def archive_bytes(files):
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, raw in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 10, 4, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o40755 if name.endswith('/') else 0o100644) << 16
            archive.writestr(info, raw)
    return output.getvalue()


def prepare_payloads(config, source, backup, output):
    approved = source / 'release/plus24'
    dll = (approved / 'Mouse_Injector.dll').read_bytes()
    require(len(dll) == 163328 and sha(dll) == DLL_SHA, 'Not the user-confirmed DLL')
    manifest_raw = (approved / 'build-manifest.json').read_bytes()
    manifest = json.loads(manifest_raw)
    for name, digest in manifest['sources_sha256'].items():
        require(sha((source / name).read_bytes()) == digest, 'Build input mismatch: ' + name)
    require(b'Mouse Injector for GE/PD ' in dll, 'Missing generic injector identity')
    require('Mouse Injector - Input Settings'.encode('utf-16-le') in dll, 'Incorrect settings caption')
    require(b'Mouse Injector - FreeFly' not in dll, 'Unexpected plugin label')
    before = config['before']
    assets = {a['name']: a for a in before['assets']}
    package = config['package']
    old_raw = (backup / package).read_bytes()
    require(sha(old_raw) == assets[package]['digest'][7:], 'Wrong package baseline')
    previous = zipfiles(old_raw)
    name = 'plugin/Mouse_Injector.dll' if config['kind'] == 'emulator' else 'Mouse_Injector.dll'
    require(sha(previous[name]) == OLD_DLL_SHA, 'Unexpected old DLL')
    files = dict(previous)
    files[name] = dll
    payloads = {}
    if config['kind'] == 'emulator':
        require(sha(previous['1964.exe']) == EXE_SHA, 'Unexpected emulator executable')
        files['injector-build-manifest.json'] = manifest_raw
        info = previous['BUILD-INFO.txt'].decode('utf-8-sig')
        info = ''.join(line for line in info.splitlines(keepends=True) if not line.startswith('Injector '))
        info += ('Injector repository commit: ' + config['source_commit'] + '\n'
                 'Injector bytes: 163328\nInjector SHA-256: ' + DLL_SHA + '\n'
                 'Injector toolchain: Zig 0.13.0, x86-windows-gnu\n'
                 'Injector validation: production input simulation, 36 MIPS reload cases, Perfect Dark compatibility checks; user confirmed candidate works.\n')
        files['BUILD-INFO.txt'] = info.encode()
        files['SHA256SUMS.txt'] = (EXE_SHA + '  1964.exe\n' + DLL_SHA + '  plugin/Mouse_Injector.dll\n').encode()
        allowed = {name, 'injector-build-manifest.json', 'BUILD-INFO.txt', 'SHA256SUMS.txt'}
        require(files.keys() == previous.keys(), 'Package entries changed')
        require(all(files[n] == raw for n, raw in previous.items() if n not in allowed), 'Unrelated emulator file changed')
        source_files = {}
        for path in sorted(source.rglob('*')):
            if not path.is_file():
                continue
            relative = path.relative_to(source).as_posix()
            if relative.startswith(('.git/', '.github/')) or relative == 'release/plus24/Mouse_Injector.dll':
                continue
            require(path.suffix.lower() not in ('.z64', '.n64', '.v64', '.sav', '.ini', '.dmp'), 'Unexpected game/private data')
            source_files[relative] = path.read_bytes()
        source_files['SOURCE-COMMIT.txt'] = (config['source_commit'] + '\n').encode()
        payloads[INPUT_SOURCE] = archive_bytes(source_files)
    else:
        require(set(previous) == {'Mouse_Injector.dll'}, 'Standalone package entries changed')
    payloads[package] = archive_bytes(files)
    sums = sha(payloads[package]) + '  ' + package + '\n'
    if config['kind'] == 'emulator':
        sums += files['SHA256SUMS.txt'].decode()
        sums += sha((backup / EMU_SOURCE).read_bytes()) + '  ' + EMU_SOURCE + '\n'
        sums += sha(payloads[INPUT_SOURCE]) + '  ' + INPUT_SOURCE + '\n'
    else:
        sums += DLL_SHA + '  Mouse_Injector.dll\n'
    payloads['SHA256SUMS.txt'] = sums.encode()
    old_hash = assets[package]['digest'][7:]
    body = before['body']
    require(body.count(old_hash) == 1 and body.count(OLD_DLL_SHA) == 1, 'Unexpected release hash fields')
    new_body = body.replace(old_hash, sha(payloads[package])).replace(OLD_DLL_SHA, DLL_SHA)
    require(new_body.replace(sha(payloads[package]), old_hash).replace(DLL_SHA, OLD_DLL_SHA) == body, 'Non-hash description change')
    output.mkdir(parents=True, exist_ok=True)
    for n, raw in payloads.items():
        (output / n).write_bytes(raw)
    report = {'source_commit': config['source_commit'], 'dll_sha256': DLL_SHA,
              'payloads': {n: sha(raw) for n, raw in payloads.items()}, 'description_changes': 'hashes only'}
    (output / 'prepared.json').write_text(json.dumps(report, indent=2) + '\n')
    (output / 'new-body.json').write_text(json.dumps(new_body))
    return report


def check_release(config, current):
    before = config['before']
    for field in ('id', 'tag_name', 'name', 'body', 'draft', 'prerelease', 'target_commitish', 'immutable'):
        require(current.get(field) == before.get(field), 'Release metadata changed: ' + field)
    expected = {(a['name'], a['id'], a['digest'], a['size']) for a in before['assets']}
    observed = {(a['name'], a['id'], a['digest'], a['size']) for a in current['assets']}
    require(expected <= observed, 'Release assets changed since preparation')


def prepare(config, source, output):
    require(subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() == config['source_commit'], 'Wrong source checkout')
    current = api(config, '/releases/' + str(config['before']['id']))
    check_release(config, current)
    backup = output / 'previous'
    backup.mkdir(parents=True, exist_ok=True)
    (backup / 'release.json').write_text(json.dumps(current, indent=2) + '\n')
    tag = api(config, '/git/ref/tags/' + config['before']['tag_name'])
    require(tag['object'] == config['tag_object'], 'Release tag moved')
    (backup / 'tag.json').write_text(json.dumps(tag, indent=2) + '\n')
    for asset in current['assets']:
        require(Path(asset['name']).name == asset['name'], 'Unsafe asset name')
        (backup / asset['name']).write_bytes(download(config, asset, asset['digest'][7:]))
    report = prepare_payloads(config, source, backup, output)
    require(report['payloads'] == config['expected_payloads'], 'Prepared payload differs from reviewed package')
    print(json.dumps(report, indent=2))


def publish(config, output):
    require(os.environ.get('GITHUB_REPOSITORY') == config['repo'], 'Wrong repository')
    require(os.environ.get('GITHUB_REF_NAME') == 'automatic-mod-compatibility', 'Wrong publication branch')
    rid = str(config['before']['id'])
    current = api(config, '/releases/' + rid)
    check_release(config, current)
    assets = {a['name']: a for a in current['assets']}
    require(api(config, '/git/ref/tags/' + config['before']['tag_name'])['object'] == config['tag_object'], 'Release tag moved')
    payloads = {n: (output / n).read_bytes() for n in config['expected_payloads']}
    require(all(sha(raw) == config['expected_payloads'][n] for n, raw in payloads.items()), 'Prepared payload changed')
    body = current['body']
    new_body = json.loads((output / 'new-body.json').read_text())
    run = os.environ['GITHUB_RUN_ID']
    staged, moved_old, moved_new = [], [], []
    try:
        for name, raw in payloads.items():
            query = urllib.parse.urlencode({'name': 'pending-' + run + '-' + name})
            request = urllib.request.Request('https://uploads.github.com/repos/' + config['repo'] + '/releases/' + rid + '/assets?' + query,
                data=raw, method='POST', headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                'Content-Type': 'application/zip' if name.endswith('.zip') else 'text/plain',
                'User-Agent': 'Mouse-Injector-release-maintenance'})
            with urllib.request.urlopen(request, timeout=60) as response:
                asset = json.load(response)
            staged.append((name, asset))
            require(asset['size'] == len(raw) and asset.get('digest') == 'sha256:' + sha(raw), 'Upload digest mismatch')
            download(config, asset, sha(raw))
        check_release(config, api(config, '/releases/' + rid))
        for name, asset in staged:
            old = assets[name]
            api(config, '/releases/assets/' + str(old['id']), 'PATCH', {'name': 'previous-' + run + '-' + name})
            moved_old.append((name, old))
            api(config, '/releases/assets/' + str(asset['id']), 'PATCH', {'name': name})
            moved_new.append((name, asset))
        api(config, '/releases/' + rid, 'PATCH', {'body': new_body})
        final = api(config, '/releases/' + rid)
        require(final['body'] == new_body, 'Published body mismatch')
        for field in ('tag_name', 'name', 'draft', 'prerelease', 'target_commitish'):
            require(final[field] == current[field], 'Release identity changed')
        require(api(config, '/git/ref/tags/' + config['before']['tag_name'])['object'] == config['tag_object'], 'Release tag changed')
        for name, raw in payloads.items():
            asset = next(a for a in final['assets'] if a['name'] == name)
            require(asset.get('digest') == 'sha256:' + sha(raw), 'Final asset digest mismatch')
            downloaded = download(config, asset, sha(raw))
            if name == config['package']:
                entries = zipfiles(downloaded)
                key = 'plugin/Mouse_Injector.dll' if config['kind'] == 'emulator' else 'Mouse_Injector.dll'
                require(sha(entries[key]) == DLL_SHA, 'Published DLL mismatch')
                if config['kind'] == 'emulator':
                    require(sha(entries['1964.exe']) == EXE_SHA, 'Published emulator changed')
    except Exception:
        for name, asset in reversed(moved_new):
            api(config, '/releases/assets/' + str(asset['id']), 'PATCH', {'name': 'failed-' + run + '-' + name})
        for name, old in reversed(moved_old):
            api(config, '/releases/assets/' + str(old['id']), 'PATCH', {'name': name})
        latest = api(config, '/releases/' + rid)
        if latest['body'] == new_body:
            api(config, '/releases/' + rid, 'PATCH', {'body': body})
        for _, asset in staged:
            api(config, '/releases/assets/' + str(asset['id']), 'DELETE')
        raise
    for _, old in moved_old:
        api(config, '/releases/assets/' + str(old['id']), 'DELETE')
    final = api(config, '/releases/' + rid)
    require({a['name'] for a in final['assets']} == set(assets), 'Unexpected final release assets')
    report = json.loads((output / 'prepared.json').read_text())
    report.update(release=final['html_url'], public_downloads_verified=True, release_tag_unchanged=True,
                  user_confirmed_dll_preserved=True, emulator_unchanged=config['kind'] == 'emulator')
    (output / 'publication.json').write_text(json.dumps(report, indent=2) + '\n')
    (output / 'release-after.json').write_text(json.dumps(final, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['prepare', 'publish'])
    parser.add_argument('--config', required=True)
    parser.add_argument('--source', default='injector-source')
    parser.add_argument('--output', default='release-out')
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text())
    if args.mode == 'prepare':
        prepare(config, Path(args.source), Path(args.output))
    else:
        publish(config, Path(args.output))
