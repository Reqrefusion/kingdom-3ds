#!/usr/bin/env python3
"""Publish the built application using GitHub CLI on Windows or Linux."""
import os
import re
import subprocess
from pathlib import Path

NOTES = 'Copy kingdom.3dsx to SD:/3ds/. The images, music and sounds of the original open-source Kingdom are inside the file. Non-commercial use only: see LICENSE.'


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def main():
    repo = os.environ['GITHUB_REPOSITORY']
    sha = os.environ.get('RELEASE_SHA') or os.environ['GITHUB_SHA']
    title = os.environ.get('RELEASE_NAME', '').strip() or 'latest'
    tag = re.sub(r'\s', '-', title)
    if tag.lower() == 'latest':
        tag = title = 'latest'
    run('git', 'check-ref-format', 'refs/tags/' + tag)
    artifact = Path('release/kingdom.3dsx')
    if not artifact.is_file() or artifact.stat().st_size == 0:
        raise SystemExit('Missing or empty release/kingdom.3dsx')
    if tag == 'latest':
        # Distinguish an absent release from network/authentication errors.
        releases = run('gh', 'api', '--paginate', f'repos/{repo}/releases',
                       '--jq', '.[].tag_name', capture_output=True, text=True)
        if tag in releases.stdout.splitlines():
            run('gh', 'release', 'upload', tag, str(artifact), '--repo', repo, '--clobber')
            run('gh', 'release', 'edit', tag, '--repo', repo, '--target', sha,
                '--title', title, '--notes', NOTES, '--prerelease', '--latest=false')
            run('gh', 'api', '--method', 'PATCH', f'repos/{repo}/git/refs/tags/latest',
                '-f', f'sha={sha}', '-F', 'force=true')
            return
        run('gh', 'release', 'create', tag, str(artifact), '--repo', repo,
            '--target', sha, '--title', title, '--notes', NOTES, '--prerelease', '--latest=false')
    else:
        run('gh', 'release', 'create', '--repo', repo, '--target', sha, '--title', title,
            '--notes', NOTES, '--latest', '--', tag, str(artifact))


if __name__ == '__main__':
    main()
