"""Publish reviewed release payloads without rebuilding binaries or moving tags.

prepare/validate are offline; backup is read-only; publish requires the backup
artifact uploaded by the companion workflow. Existing, unlisted assets remain.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import io
import json
import os
import re
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
import zipfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def safe_name(name):
    require(isinstance(name, str) and name not in ('', '.', '..')
            and '/' not in name and '\\' not in name
            and not any(ord(c) < 32 for c in name), 'Unsafe asset name')


def source_path(source, name, payload=False):
    require(isinstance(name, str), 'Expected relative file path')
    relative = PurePosixPath(name)
    require(not relative.is_absolute() and '..' not in relative.parts,
            'Unsafe source path: ' + name)
    path = (source / name).resolve()
    require(path.is_relative_to(source.resolve()), 'Path leaves source tree')
    if payload:
        require(path.is_relative_to((source / 'release/pd-beta/payloads').resolve()),
                'Payload must be in release/pd-beta/payloads')
    require(path.is_file(), 'Missing file: ' + name)
    return path


def validate_zip(raw, expected_members):
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        names = archive.namelist()
        require(len(names) == len(set(names)), 'Duplicate ZIP entries')
        for name in names:
            path = PurePosixPath(name)
            require(not path.is_absolute() and '..' not in path.parts
                    and '\\' not in name and not re.match(r'^[A-Za-z]:', name),
                    'Unsafe ZIP member: ' + name)
        require(archive.testzip() is None, 'Corrupt ZIP')
        for name, digest in expected_members.items():
            require(name in names and sha(archive.read(name)) == digest,
                    'ZIP member mismatch: ' + name)


def load_plan(config_path, source):
    config_raw = config_path.read_bytes()
    config = json.loads(config_raw)
    require(config['schema_version'] == 1, 'Unsupported config schema')
    require(re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', config['repo']),
            'Invalid repository')
    require(config['branch'] == 'automatic-mod-compatibility', 'Unexpected branch')
    require(config['kind'] in ('emulator', 'injector'), 'Unknown package kind')
    require(isinstance(config['before_latest_release_id'], int), 'Missing previous latest release')
    require(config['targets'], 'No release targets')
    targets, seen_ids, inputs = [], set(), {}
    for index, entry in enumerate(config['targets']):
        before_path = source_path(source, entry['before_file'])
        body_path = source_path(source, entry['body_file'])
        before_raw, body_raw = before_path.read_bytes(), body_path.read_bytes()
        before = json.loads(before_raw)
        require(before['id'] not in seen_ids, 'Duplicate release target')
        seen_ids.add(before['id'])
        require(not before['draft'] and not before['prerelease'] and not before.get('immutable'),
                'Only mutable, published stable releases are supported')
        require(entry['make_latest'] in (True, False), 'make_latest must be boolean')
        require(entry['name'] and isinstance(entry['name'], str), 'Missing release name')
        require(entry['tag_object']['type'] in ('commit', 'tag')
                and re.fullmatch(r'[0-9a-f]{40}', entry['tag_object']['sha']), 'Invalid tag object')
        assets = before['assets']
        require(len({a['name'] for a in assets}) == len(assets), 'Duplicate existing asset names')
        for asset in assets:
            safe_name(asset['name'])
        inputs[entry['before_file']], inputs[entry['body_file']] = sha(before_raw), sha(body_raw)
        payloads, names, has_package = [], set(), False
        for payload in entry['payloads']:
            safe_name(payload['name'])
            require(payload['name'] not in names, 'Duplicate payload name')
            names.add(payload['name'])
            path = source_path(source, payload['path'], payload=True)
            raw = path.read_bytes()
            require(re.fullmatch(r'[0-9a-f]{64}', payload['sha256'])
                    and sha(raw) == payload['sha256'], 'Payload mismatch: ' + payload['name'])
            members = payload.get('zip_members', {})
            if payload['name'].lower().endswith('.zip'):
                validate_zip(raw, members)
            else:
                require(not members, 'zip_members requires a ZIP payload')
            if members:
                required = {'1964.exe', 'plugin/Mouse_Injector.dll'} if config['kind'] == 'emulator' else {'Mouse_Injector.dll'}
                require(required <= set(members), 'Package must verify its exact EXE/DLL members')
                has_package = True
            payloads.append(dict(payload, bytes=len(raw), local_path=str(path)))
            inputs[payload['path']] = sha(raw)
        require(has_package, 'Each release needs a ZIP package with binary member hashes')
        removals = entry.get('remove_assets', [])
        require(isinstance(removals, list) and len(set(removals)) == len(removals),
                'Invalid duplicate removal list')
        for name in removals:
            safe_name(name)
            require(name in {a['name'] for a in assets}, 'Removal not present in reviewed snapshot')
            require(name not in names, 'Cannot remove and replace the same asset')
        targets.append(dict(entry, before=before, body=body_raw.decode('utf-8'),
                            payloads=payloads, key=str(before['id'])))
    require(sum(t['make_latest'] for t in targets) == 1, 'Exactly one target must become latest')
    plan = {'config_sha256': sha(config_raw), 'repo': config['repo'],
            'inputs_sha256': inputs,
            'targets': [{'id': t['before']['id'], 'tag': t['before']['tag_name'],
                         'name': t['name'], 'make_latest': t['make_latest'],
                         'remove_assets': t.get('remove_assets', []),
                         'payloads': {p['name']: p['sha256'] for p in t['payloads']}}
                        for t in targets]}
    return config, targets, plan


def api(config, suffix, method='GET', data=None):
    require(suffix.startswith('/'), 'Invalid API suffix')
    request = urllib.request.Request('https://api.github.com/repos/' + config['repo'] + suffix,
        data=None if data is None else json.dumps(data).encode(), method=method,
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                 'Accept': 'application/vnd.github+json', 'Content-Type': 'application/json',
                 'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': '1964GEPD-release-maintenance'})
    with urllib.request.urlopen(request, timeout=60) as response:
        raw = response.read()
    return json.loads(raw) if raw else None


def current_release(config, release_id):
    release = api(config, '/releases/' + str(release_id))
    assets, page = [], 1
    while True:
        batch = api(config, '/releases/' + str(release_id) + '/assets?per_page=100&page=' + str(page))
        assets.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    release['assets'] = assets
    return release


def asset_identity(asset):
    return (asset['id'], asset['name'], asset['size'], asset.get('digest'), asset.get('label'))


def check_before(target, current, staged_ids=()):
    before = target['before']
    for field in ('id', 'tag_name', 'name', 'body', 'draft', 'prerelease',
                  'target_commitish', 'created_at', 'published_at', 'immutable'):
        require(current.get(field) == before.get(field), 'Release changed: ' + field)
    expected = {asset_identity(a) for a in before['assets']}
    actual = {asset_identity(a) for a in current['assets'] if a['id'] not in staged_ids}
    require(actual == expected, 'Release asset inventory changed')


def check_tag(config, target):
    tag = api(config, '/git/ref/tags/' + urllib.parse.quote(target['before']['tag_name'], safe=''))
    require(tag['object'] == target['tag_object'], 'Release tag moved')
    return tag


def download(config, target, asset, expected=None):
    parsed = urllib.parse.urlparse(asset['browser_download_url'])
    prefix = '/' + config['repo'] + '/releases/download/' + target['before']['tag_name'] + '/'
    require(parsed.scheme == 'https' and parsed.netloc == 'github.com'
            and urllib.parse.unquote(parsed.path).startswith(prefix), 'Unexpected asset URL')
    observed = []
    for delay in (0, 1, 2, 4, 8, 16):
        if delay:
            time.sleep(delay)
        query = urllib.parse.urlencode({'asset': asset['id'], 'sha256': expected or '', 'nonce': time.time_ns()})
        request = urllib.request.Request(asset['browser_download_url'] + '?' + query,
            headers={'Cache-Control': 'no-cache', 'User-Agent': '1964GEPD-release-verification'})
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                raw = response.read()
            observed.append(sha(raw))
            if len(raw) == asset['size'] and (expected is None or sha(raw) == expected):
                return raw
        except urllib.error.HTTPError as error:
            if error.code not in (404, 429, 502, 503, 504):
                raise
            observed.append(str(error.code))
    raise RuntimeError('Public download mismatch: ' + asset['name'] + ': ' + repr(observed))


def backup(config, targets, plan, output):
    require(api(config, '/releases/latest')['id'] == config['before_latest_release_id'],
            'Latest release changed since review')
    records = []
    for target in targets:
        current = current_release(config, target['before']['id'])
        check_before(target, current)
        tag = check_tag(config, target)
        folder = output / 'previous' / target['key']
        write_json(folder / 'release.json', current)
        write_json(folder / 'tag.json', tag)
        hashes = {}
        for asset in current['assets']:
            digest = asset.get('digest')
            expected = digest[7:] if digest and digest.startswith('sha256:') else None
            raw = download(config, target, asset, expected)
            path = folder / 'assets' / asset['name']
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            hashes[asset['name']] = sha(raw)
        records.append({'id': current['id'], 'assets_sha256': hashes,
                        'release_sha256': sha((folder / 'release.json').read_bytes()),
                        'tag_sha256': sha((folder / 'tag.json').read_bytes())})
        print('Backed up ' + current['tag_name'], flush=True)
    write_json(output / 'previous/backup-manifest.json', {'plan': plan, 'releases': records})
    write_json(output / 'prepared.json', plan)


def check_backup(config, targets, plan, output):
    manifest = json.loads((output / 'previous/backup-manifest.json').read_text())
    require(manifest['plan'] == plan, 'Backup plan differs from reviewed payloads')
    require({r['id'] for r in manifest['releases']} == {t['before']['id'] for t in targets},
            'Missing release backup')
    for record in manifest['releases']:
        folder = output / 'previous' / str(record['id'])
        for filename, field in [('release.json', 'release_sha256'), ('tag.json', 'tag_sha256')]:
            require(sha((folder / filename).read_bytes()) == record[field], 'Backup metadata changed')
        for name, digest in record['assets_sha256'].items():
            safe_name(name)
            require(sha((folder / 'assets' / name).read_bytes()) == digest, 'Backup asset changed')
    artifact_id = os.environ.get('BACKUP_ARTIFACT_ID', '')
    require(artifact_id.isdecimal(), 'No uploaded backup artifact receipt')
    artifact = api(config, '/actions/artifacts/' + artifact_id)
    require(not artifact['expired'] and artifact['name'] == os.environ['BACKUP_ARTIFACT_NAME'],
            'Backup artifact unavailable')
    if artifact.get('digest'):
        require(artifact['digest'].removeprefix('sha256:') == os.environ['BACKUP_ARTIFACT_DIGEST'].removeprefix('sha256:'),
                'Backup artifact digest differs')
    return {'id': int(artifact_id), 'name': artifact['name'], 'digest': artifact.get('digest')}


def upload(config, target, name, raw):
    query = urllib.parse.urlencode({'name': name})
    url = ('https://uploads.github.com/repos/' + config['repo'] + '/releases/'
           + str(target['before']['id']) + '/assets?' + query)
    request = urllib.request.Request(url, data=raw, method='POST',
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                 'Content-Type': 'application/zip' if name.endswith('.zip') else 'application/octet-stream',
                 'User-Agent': '1964GEPD-release-maintenance'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def rollback(config, targets, staged, metadata_changed, output, run):
    errors = []
    # Removing only this run's staged uploads frees all canonical names.
    for record in reversed(staged):
        try:
            api(config, '/releases/assets/' + str(record['asset']['id']), 'DELETE')
        except Exception as error:
            errors.append('Remove staged asset: ' + str(error))
    for target in reversed(targets):
        try:
            current = current_release(config, target['before']['id'])
            current_by_id = {a['id']: a for a in current['assets']}
            for old in target['before']['assets']:
                found = current_by_id.get(old['id'])
                require(found is not None, 'Original asset missing during rollback')
                if found['name'] != old['name']:
                    require(found['name'] == 'previous-' + run + '-' + old['name'],
                            'Unexpected concurrent asset rename; not overwriting it')
                    api(config, '/releases/assets/' + str(old['id']), 'PATCH', {'name': old['name']})
            if target['key'] in metadata_changed:
                require(current['name'] in (target['name'], target['before']['name'])
                        and current['body'] in (target['body'], target['before']['body']),
                        'Unexpected concurrent metadata change; not overwriting it')
                api(config, '/releases/' + target['key'], 'PATCH',
                    {'name': target['before']['name'], 'body': target['before']['body']})
            check_tag(config, target)
            check_before(target, current_release(config, target['before']['id']))
        except Exception as error:
            errors.append('Restore release ' + target['key'] + ': ' + str(error))
    try:
        api(config, '/releases/' + str(config['before_latest_release_id']), 'PATCH', {'make_latest': 'true'})
        require(api(config, '/releases/latest')['id'] == config['before_latest_release_id'], 'Latest release not restored')
    except Exception as error:
        errors.append('Restore latest: ' + str(error))
    write_json(output / 'rollback.json', {'complete': not errors, 'errors': errors})
    return errors


def publish(config, targets, plan, output):
    require(os.environ.get('GITHUB_REPOSITORY') == config['repo'], 'Wrong repository')
    require(os.environ.get('GITHUB_REF_NAME') == config['branch'], 'Wrong publication branch')
    artifact = check_backup(config, targets, plan, output)
    require(api(config, '/releases/latest')['id'] == config['before_latest_release_id'], 'Latest release changed')
    for target in targets:
        check_before(target, current_release(config, target['before']['id']))
        check_tag(config, target)
    run = os.environ['GITHUB_RUN_ID'] + '-' + uuid.uuid4().hex[:8]
    staged, moved_old, metadata_changed = [], [], set()
    try:
        # Stage and publicly verify every target before displacing any old file.
        for target in targets:
            for payload in target['payloads']:
                raw = Path(payload['local_path']).read_bytes()
                require(sha(raw) == payload['sha256'], 'Local payload changed')
                name = 'pending-' + run + '-' + payload['name']
                require(len(name.encode()) <= 255, 'Staged asset name too long')
                asset = upload(config, target, name, raw)
                staged.append({'target': target, 'payload': payload, 'asset': asset})
                require(asset['size'] == len(raw) and asset.get('digest') == 'sha256:' + payload['sha256'],
                        'Uploaded digest mismatch')
                download(config, target, asset, payload['sha256'])
                print('Verified staged ' + payload['name'], flush=True)
        for target in targets:
            allowed = {r['asset']['id'] for r in staged if r['target']['key'] == target['key']}
            check_before(target, current_release(config, target['before']['id']), allowed)
            check_tag(config, target)
        # Explicitly superseded aliases are backed up and retained under a
        # temporary name until every replacement download has been verified.
        for target in targets:
            for name in target.get('remove_assets', []):
                old = next(a for a in target['before']['assets'] if a['name'] == name)
                api(config, '/releases/assets/' + str(old['id']), 'PATCH',
                    {'name': 'previous-' + run + '-' + old['name']})
                moved_old.append({'target': target, 'asset': old})
        for record in staged:
            target, payload, asset = record['target'], record['payload'], record['asset']
            old = next((a for a in target['before']['assets'] if a['name'] == payload['name']), None)
            if old is not None:
                api(config, '/releases/assets/' + str(old['id']), 'PATCH',
                    {'name': 'previous-' + run + '-' + old['name']})
                moved_old.append({'target': target, 'asset': old})
            api(config, '/releases/assets/' + str(asset['id']), 'PATCH', {'name': payload['name']})
        # Set the canonical latest target last.
        for target in sorted(targets, key=lambda t: t['make_latest']):
            metadata_changed.add(target['key'])
            api(config, '/releases/' + target['key'], 'PATCH',
                {'name': target['name'], 'body': target['body'],
                 'make_latest': 'true' if target['make_latest'] else 'false'})
        for target in targets:
            final = current_release(config, target['before']['id'])
            require(final['body'] == target['body'] and final['name'] == target['name'], 'Release text mismatch')
            for field in ('id', 'tag_name', 'draft', 'prerelease', 'target_commitish', 'created_at', 'published_at', 'immutable'):
                require(final.get(field) == target['before'].get(field), 'Release identity/date changed: ' + field)
            check_tag(config, target)
            by_name = {a['name']: a for a in final['assets']}
            replacement_names = {p['name'] for p in target['payloads']} | set(target.get('remove_assets', []))
            for old in target['before']['assets']:
                if old['name'] not in replacement_names:
                    require(asset_identity(by_name[old['name']]) == asset_identity(old), 'Unrelated asset changed')
            for payload in target['payloads']:
                asset = by_name[payload['name']]
                require(asset.get('digest') == 'sha256:' + payload['sha256'], 'Final asset digest mismatch')
                raw = download(config, target, asset, payload['sha256'])
                if payload['name'].lower().endswith('.zip'):
                    validate_zip(raw, payload.get('zip_members', {}))
            print('Verified published ' + final['tag_name'], flush=True)
        latest_id = next(t['before']['id'] for t in targets if t['make_latest'])
        require(api(config, '/releases/latest')['id'] == latest_id, 'Canonical latest release not selected')
    except Exception as error:
        rollback_errors = rollback(config, targets, staged, metadata_changed, output, run)
        raise RuntimeError('Publication failed: ' + str(error) + '; rollback errors: ' + repr(rollback_errors)) from error
    # Commit point: all new public bytes and metadata verified; old bytes are
    # retained in the uploaded backup artifact before deletion starts.
    cleanup_errors = []
    for record in moved_old:
        try:
            api(config, '/releases/assets/' + str(record['asset']['id']), 'DELETE')
        except Exception as error:
            cleanup_errors.append({'id': record['asset']['id'], 'name': record['asset']['name'], 'error': str(error)})
    report = {'repo': config['repo'], 'backup_artifact': artifact, 'public_downloads_verified': True,
              'tags_and_release_dates_unchanged': True, 'cleanup_errors': cleanup_errors, 'releases': []}
    for target in targets:
        final = current_release(config, target['before']['id'])
        write_json(output / ('release-after-' + target['key'] + '.json'), final)
        expected = ({a['name'] for a in target['before']['assets']} - set(target.get('remove_assets', []))) | {p['name'] for p in target['payloads']}
        if not cleanup_errors:
            require({a['name'] for a in final['assets']} == expected, 'Unexpected final release inventory')
        report['releases'].append({'id': final['id'], 'url': final['html_url'],
                                   'payloads': {p['name']: p['sha256'] for p in target['payloads']}})
    write_json(output / 'publication.json', report)
    print(json.dumps(report, indent=2), flush=True)
    if cleanup_errors:
        raise RuntimeError('Publication is verified; cleanup of displaced old assets needs attention. See publication.json.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('validate', 'prepare', 'backup', 'publish'))
    parser.add_argument('--config', default='release/pd-beta/publication.json')
    parser.add_argument('--source', default='.')
    parser.add_argument('--output', default='release-out')
    args = parser.parse_args()
    source, output = Path(args.source).resolve(), Path(args.output).resolve()
    config_path = source_path(source, args.config)
    config, targets, plan = load_plan(config_path, source)
    if args.mode in ('validate', 'prepare'):
        write_json(output / 'prepared.json', plan)
        print(json.dumps(plan, indent=2))
    elif args.mode == 'backup':
        backup(config, targets, plan, output)
    else:
        publish(config, targets, plan, output)


if __name__ == '__main__':
    main()
