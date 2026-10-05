"""Export an Actions build as content-addressed Git blobs, without a release.

The workspace can retrieve JSON Git blobs through its GitHub connector. No
branch, tag, release, or repository file is updated by this export step.
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import urllib.request
import zipfile

candidate = Path('candidate')
archive = Path('pd-beta-emulator-build.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for p in sorted(candidate.iterdir()):
        if p.is_file() and p.suffix != '.binlog':
            z.write(p, p.name)
data = archive.read_bytes()
chunks = []
for offset in range(0, len(data), 180000):
    chunk = data[offset:offset+180000]
    body = json.dumps({'encoding': 'base64', 'content': base64.b64encode(chunk).decode('ascii')}).encode()
    request = urllib.request.Request(
        'https://api.github.com/repos/' + os.environ['GITHUB_REPOSITORY'] + '/git/blobs',
        data=body, method='POST', headers={
            'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
            'Accept': 'application/vnd.github+json',
            'X-GitHub-Api-Version': '2022-11-28', 'Content-Type': 'application/json'})
    with urllib.request.urlopen(request) as response:
        result = json.load(response)
    chunks.append({'sha': result['sha'], 'bytes': len(chunk),
                   'sha256': hashlib.sha256(chunk).hexdigest()})
manifest = {'commit': os.environ['GITHUB_SHA'], 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest(), 'chunks': chunks}
print('PD_CANDIDATE_TRANSFER=' + json.dumps(manifest, separators=(',', ':')))
