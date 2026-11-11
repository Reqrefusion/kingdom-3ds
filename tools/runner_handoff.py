#!/usr/bin/env python3
"""Atomic ownership gate shared by Linux builds and Windows takeovers.

Annotated claim tags are immutable for the lifetime of a source run/attempt.
They contain ownership metadata, not new commits or application releases.
"""
import json
import os
import re
import sys
import time
import uuid
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

PREFIX = 'kingdom-runner-claims/'
WORKFLOW = '.github/workflows/build-3dsx.yml'


class Api:
    def __init__(self, repo, token):
        self.base = os.environ.get('GITHUB_API_URL', 'https://api.github.com')
        self.repo, self.token = repo, token

    def __call__(self, method, path, data=None):
        req = Request(self.base + '/repos/' + self.repo + '/' + path,
                      data=None if data is None else json.dumps(data).encode(),
                      method=method, headers={
                          'Authorization': 'Bearer ' + self.token,
                          'Accept': 'application/vnd.github+json',
                          'Content-Type': 'application/json',
                          'User-Agent': 'kingdom-runner-handoff',
                          'X-GitHub-Api-Version': '2022-11-28'})
        with urlopen(req, timeout=30) as response:
            body = response.read()
            return json.loads(body) if body else None


def metadata(title):
    if not title.startswith('3DSX '):
        raise ValueError('Source run predates cooperative handoff')
    values = json.loads(title[5:])
    if (not isinstance(values, list) or len(values) != 2
            or not all(isinstance(v, str) for v in values)):
        raise ValueError('Invalid source metadata')
    ref, release = values
    if not ref.startswith(('refs/heads/', 'refs/tags/')):
        raise ValueError('Invalid source ref')
    if ref.startswith('refs/tags/' + PREFIX):
        raise ValueError('Claim refs cannot be built')
    return ref, release


def claim(api, run_id, attempt, sha, owner, owner_run):
    name = PREFIX + str(run_id) + '-' + str(attempt)
    record = dict(version=1, source_id=int(run_id), attempt=int(attempt),
                  sha=sha, owner=owner, owner_run=int(owner_run))
    tag = api('POST', 'git/tags', dict(tag=name, message=json.dumps(record),
                                     object=sha, type='commit'))
    try:
        api('POST', 'git/refs', dict(ref='refs/tags/' + name, sha=tag['sha']))
        return True
    except HTTPError as error:
        if error.code != 422:
            raise
        # Only an existing valid claim counts as a lost race. Other permission
        # or ruleset failures must fail closed, never allow duplicate builds.
        ref = api('GET', 'git/ref/tags/' + name)
        if ref['object']['type'] != 'tag':
            raise ValueError('Unexpected claim object')
        existing = json.loads(api('GET', 'git/tags/' + ref['object']['sha'])['message'])
        if any(existing.get(k) != record[k] for k in ('version', 'source_id', 'attempt', 'sha')):
            raise ValueError('Claim does not match source')
        if existing.get('owner') not in ('linux', 'windows'):
            raise ValueError('Unknown claim owner')
        return existing == record


def jobs(api, run_id, attempt):
    result, page = [], 1
    while True:
        batch = api('GET', f'actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100&page={page}')['jobs']
        result.extend(batch)
        if len(batch) < 100:
            return result
        page += 1


def queued(run, listing):
    if run['status'] not in ('queued', 'pending', 'in_progress'):
        return False
    builds = [j for j in listing if j['name'] == 'build']
    if builds:
        return len(builds) == 1 and builds[0]['status'] == 'queued'
    return not listing and run['status'] in ('queued', 'pending')


def source(api, run_id, repo, attempt):
    run = api('GET', f'actions/runs/{run_id}')
    if (run['repository']['full_name'].lower() != repo.lower()
            or run['head_repository']['full_name'].lower() != repo.lower()
            or run['path'].split('@')[0] != WORKFLOW
            or run['event'] not in ('push', 'workflow_dispatch')
            or int(run['run_attempt']) != int(attempt)
            or not re.fullmatch(r'[0-9a-f]{40}', run['head_sha'])):
        raise ValueError('Source is not eligible for trusted Windows execution')
    ref, release = metadata(run['display_title'])
    if ref.split('/', 2)[2] != run['head_branch']:
        raise ValueError('Source ref does not match run')
    return run, ref, release


def skipped_for_windows(api, run_id, attempt):
    listing = jobs(api, run_id, attempt)
    builds = [j for j in listing if j['name'] == 'build']
    if len(builds) != 1:
        return False
    steps = builds[0].get('steps', [])
    guards = [i for i, s in enumerate(steps) if s['name'] == 'Claim Linux build']
    if len(guards) != 1:
        return False
    i = guards[0]
    work = [s for s in steps[i + 1:] if not s['name'].startswith(('Post ', 'Complete job'))]
    return (steps[i].get('conclusion') == 'success' and bool(work)
            and all(s.get('conclusion') == 'skipped' for s in work)
            and all(j.get('conclusion') == 'skipped' for j in listing if j['name'] != 'build'))


def stop_source(api, run_id, attempt, sleep=time.sleep):
    # Cancellation follows ownership. If Linux starts checkout during the race,
    # its gate cannot pass and compilation/publication cannot start there.
    current = api('GET', f'actions/runs/{run_id}')
    if int(current['run_attempt']) != int(attempt):
        raise ValueError('Source was rerun; leaving the new attempt alone')
    if current['status'] == 'completed':
        return skipped_for_windows(api, run_id, attempt)
    accepted = False
    try:
        api('POST', f'actions/runs/{run_id}/cancel')
        accepted = True
    except HTTPError as error:
        if error.code != 409:
            raise
    for _ in range(30):
        run = api('GET', f'actions/runs/{run_id}')
        if int(run['run_attempt']) != int(attempt):
            raise ValueError('Source was rerun; leaving the new attempt alone')
        if run['status'] == 'completed':
            if accepted and run.get('conclusion') == 'cancelled':
                return True
            return skipped_for_windows(api, run_id, attempt)
        if not accepted:
            raise RuntimeError('Source cancellation was not accepted')
        sleep(2)
    raise RuntimeError('Cancellation unconfirmed; refusing duplicate build')


def linux(api, env):
    if env['GITHUB_EVENT_NAME'] == 'pull_request':
        return {'proceed': 'true'}
    if env['GITHUB_EVENT_NAME'] not in ('push', 'workflow_dispatch'):
        raise ValueError('Unsupported Linux event')
    if env['GITHUB_REF'].startswith('refs/tags/' + PREFIX):
        return {'proceed': 'false'}
    won = claim(api, int(env['GITHUB_RUN_ID']), int(env['GITHUB_RUN_ATTEMPT']),
                env['GITHUB_SHA'], 'linux', int(env['GITHUB_RUN_ID']))
    return {'proceed': str(won).lower()}


def windows(api, env, event, sleep=time.sleep):
    if env['GITHUB_EVENT_NAME'] == 'workflow_dispatch':
        return dict(proceed='true', sha=env['GITHUB_SHA'], publish='true',
                    release_name=event.get('inputs', {}).get('release_tag', ''))
    if env['GITHUB_EVENT_NAME'] != 'workflow_run':
        raise ValueError('Unsupported Windows event')
    trigger = event['workflow_run']
    run_id, attempt = int(trigger['id']), int(trigger['run_attempt'])
    run, ref, release = source(api, run_id, env['GITHUB_REPOSITORY'], attempt)
    if not queued(run, jobs(api, run_id, attempt)):
        return {'proceed': 'false'}
    if not claim(api, run_id, attempt, run['head_sha'], 'windows', int(env['GITHUB_RUN_ID'])):
        return {'proceed': 'false'}
    if not stop_source(api, run_id, attempt, sleep):
        return {'proceed': 'false'}
    if run['event'] == 'push' and ref.startswith('refs/tags/'):
        release = ref[len('refs/tags/'):]
    default = env['DEFAULT_BRANCH']
    publish = (run['event'] == 'workflow_dispatch' or ref.startswith('refs/tags/')
               or ref == 'refs/heads/' + default)
    return dict(proceed='true', sha=run['head_sha'], publish=str(publish).lower(),
                release_name=release)


def emit(values):
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as stream:
        for key, value in values.items():
            delimiter = 'handoff_' + uuid.uuid4().hex
            stream.write(f'{key}<<{delimiter}\n{value}\n{delimiter}\n')
    print('Build ownership: ' + ('acquired' if values['proceed'] == 'true' else 'not acquired; skipping'))


def main():
    env = os.environ
    api = Api(env['GITHUB_REPOSITORY'], env['GH_TOKEN'])
    if sys.argv[1:] == ['linux']:
        result = linux(api, env)
    elif sys.argv[1:] == ['windows']:
        event = json.loads(Path(env['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
        result = windows(api, env, event)
    else:
        raise SystemExit('Usage: runner_handoff.py linux|windows')
    emit(result)


if __name__ == '__main__':
    main()
