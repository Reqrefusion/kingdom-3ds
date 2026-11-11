"""Handoff races, source trust, exact revisions and publication policy (no network)."""
import copy
import io
import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import HTTPError

spec = importlib.util.spec_from_file_location('handoff', Path(__file__).parents[1] / 'tools/runner_handoff.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
SHA = 'a' * 40
REPO = 'Reqrefusion/kingdom-3ds'


def error(code):
    result = HTTPError('https://api.github.test', code, 'test', {}, io.BytesIO())
    result.close()
    return result


class FakeApi:
    def __init__(self):
        self.run = dict(id=101, run_attempt=1, head_sha=SHA, head_branch='main',
                        path=h.WORKFLOW, event='push', status='queued',
                        display_title='3DSX ["refs/heads/main",""]',
                        repository={'full_name': REPO}, head_repository={'full_name': REPO})
        self.job_list = [dict(name='build', status='queued', steps=[])]
        self.tags, self.refs, self.calls = {}, {}, []
        self.before_ref = None
        self.cancel_error = None
        self.confirm_cancel = True

    def __call__(self, method, path, data=None):
        self.calls.append((method, path, data))
        if path == 'git/tags' and method == 'POST':
            sha = str(len(self.tags) + 1)
            self.tags[sha] = data
            return {'sha': sha}
        if path == 'git/refs' and method == 'POST':
            if self.before_ref:
                hook, self.before_ref = self.before_ref, None
                hook()
            name = data['ref'][5:]
            if name in self.refs:
                raise error(422)
            self.refs[name] = data['sha']
            return {}
        if path.startswith('git/ref/'):
            name = path[len('git/ref/'):]
            if name not in self.refs:
                raise error(404)
            return {'object': {'type': 'tag', 'sha': self.refs[name]}}
        if path.startswith('git/tags/'):
            return self.tags[path.split('/')[-1]]
        if '/jobs?' in path:
            return {'jobs': copy.deepcopy(self.job_list)}
        if path.endswith('/cancel'):
            if self.cancel_error:
                raise error(self.cancel_error)
            if self.confirm_cancel:
                self.run.update(status='completed', conclusion='cancelled')
            return None
        if path == 'actions/runs/101':
            return copy.deepcopy(self.run)
        raise AssertionError((method, path, data))

    def cancelled(self):
        return any(p.endswith('/cancel') for _, p, _ in self.calls)


class HandoffTests(unittest.TestCase):
    def setUp(self):
        self.api = FakeApi()
        self.env = dict(GITHUB_EVENT_NAME='workflow_run', GITHUB_REPOSITORY=REPO,
                        GITHUB_RUN_ID='202', GITHUB_RUN_ATTEMPT='1', GITHUB_SHA='b' * 40,
                        DEFAULT_BRANCH='main')
        self.event = {'workflow_run': {'id': 101, 'run_attempt': 1}}

    def windows(self):
        return h.windows(self.api, self.env, self.event, lambda _: None)

    def linux(self):
        env = dict(self.env, GITHUB_EVENT_NAME='push', GITHUB_RUN_ID='101',
                   GITHUB_SHA=SHA, GITHUB_REF='refs/heads/main')
        return h.linux(self.api, env)

    def test_windows_claims_then_cancels_then_uses_original_sha(self):
        result = self.windows()
        self.assertEqual(result, dict(proceed='true', sha=SHA, publish='true', release_name=''))
        paths = [p for _, p, _ in self.api.calls]
        self.assertLess(paths.index('git/refs'), paths.index('actions/runs/101/cancel'))
        self.assertEqual(self.linux()['proceed'], 'false')

    def test_running_linux_is_never_cancelled(self):
        self.api.run['status'] = 'in_progress'
        self.api.job_list[0]['status'] = 'in_progress'
        self.assertEqual(self.windows()['proceed'], 'false')
        self.assertFalse(self.api.cancelled())
        self.assertFalse(self.api.refs)

    def test_completed_linux_is_never_cancelled(self):
        self.api.run['status'] = 'completed'
        self.assertEqual(self.windows()['proceed'], 'false')
        self.assertFalse(self.api.cancelled())

    def test_linux_wins_between_windows_queue_check_and_claim(self):
        self.api.before_ref = self.linux
        self.assertEqual(self.windows()['proceed'], 'false')
        self.assertFalse(self.api.cancelled())

    def test_linux_claim_survives_stale_queued_api_response(self):
        self.assertEqual(self.linux()['proceed'], 'true')
        self.assertEqual(self.windows()['proceed'], 'false')
        self.assertFalse(self.api.cancelled())

    def test_second_windows_run_cannot_build_again(self):
        self.assertEqual(self.windows()['proceed'], 'true')
        self.env['GITHUB_RUN_ID'] = '203'
        self.api.run['status'] = 'queued'
        self.assertEqual(self.windows()['proceed'], 'false')

    def test_pull_request_never_runs_on_windows(self):
        self.api.run['event'] = 'pull_request'
        with self.assertRaises(ValueError):
            self.windows()
        self.assertFalse(self.api.refs)

    def test_fork_never_runs_on_windows(self):
        self.api.run['head_repository']['full_name'] = 'someone/kingdom-3ds'
        with self.assertRaises(ValueError):
            self.windows()

    def test_unrelated_workflow_cannot_be_cancelled(self):
        self.api.run['path'] = '.github/workflows/other.yml'
        with self.assertRaises(ValueError):
            self.windows()
        self.assertFalse(self.api.cancelled())

    def test_legacy_runs_without_gate_are_not_taken_over(self):
        self.api.run['display_title'] = 'Old build'
        with self.assertRaises(ValueError):
            self.windows()
        self.assertFalse(self.api.cancelled())

    def test_mismatched_ref_is_rejected(self):
        self.api.run['head_branch'] = 'other'
        with self.assertRaises(ValueError):
            self.windows()

    def test_claim_tag_push_is_rejected(self):
        self.api.run['display_title'] = '3DSX ["refs/tags/kingdom-runner-claims/1-1",""]'
        with self.assertRaises(ValueError):
            self.windows()

    def test_non_default_branch_builds_without_publication(self):
        self.api.run.update(head_branch='feature', display_title='3DSX ["refs/heads/feature",""]')
        self.assertEqual(self.windows()['publish'], 'false')

    def test_tag_keeps_original_release_name(self):
        self.api.run.update(head_branch='v1.2', display_title='3DSX ["refs/tags/v1.2",""]')
        result = self.windows()
        self.assertEqual(result['release_name'], 'v1.2')
        self.assertEqual(result['publish'], 'true')

    def test_original_manual_release_input_is_kept(self):
        self.api.run.update(event='workflow_dispatch', display_title='3DSX ["refs/heads/main","test release"]')
        self.assertEqual(self.windows()['release_name'], 'test release')

    def test_manual_windows_still_works_without_cancel(self):
        self.env['GITHUB_EVENT_NAME'] = 'workflow_dispatch'
        self.event['inputs'] = {'release_tag': 'manual release'}
        result = self.windows()
        self.assertEqual(result['sha'], 'b' * 40)
        self.assertEqual(result['release_name'], 'manual release')
        self.assertEqual(self.api.calls, [])

    def test_fork_pr_linux_needs_no_write_api(self):
        self.env['GITHUB_EVENT_NAME'] = 'pull_request'
        self.assertEqual(h.linux(self.api, self.env)['proceed'], 'true')
        self.assertEqual(self.api.calls, [])

    def test_concurrency_queue_without_job_can_be_taken(self):
        self.api.job_list = []
        self.assertEqual(self.windows()['proceed'], 'true')

    def test_approval_wait_is_not_bypassed(self):
        self.api.run['status'] = 'waiting'
        self.assertEqual(self.windows()['proceed'], 'false')
        self.assertFalse(self.api.cancelled())

    def test_cancel_error_does_not_start_windows(self):
        self.api.cancel_error = 403
        with self.assertRaises(HTTPError):
            self.windows()

    def test_cancel_timeout_does_not_start_windows(self):
        self.api.confirm_cancel = False
        with self.assertRaises(RuntimeError):
            self.windows()

    def test_changed_attempt_is_not_cancelled(self):
        def rerun():
            self.api.run['run_attempt'] = 2
        self.api.before_ref = rerun
        with self.assertRaises(ValueError):
            self.windows()
        self.assertFalse(self.api.cancelled())

    def test_claims_are_distinct_for_reruns(self):
        self.assertTrue(h.claim(self.api, 101, 1, SHA, 'linux', 101))
        self.assertTrue(h.claim(self.api, 101, 2, SHA, 'windows', 202))
        self.assertEqual(len(self.api.refs), 2)

    def test_linux_gate_may_finish_skipping_before_cancel(self):
        def complete_gate():
            self.api.run.update(status='completed', conclusion='success')
            self.api.job_list = [dict(name='build', status='completed', conclusion='success', steps=[
                dict(name='Claim Linux build', conclusion='success'),
                dict(name='Build kingdom.3dsx', conclusion='skipped'),
                dict(name='Post actions/checkout@v7', conclusion='success')]),
                dict(name='release', status='completed', conclusion='skipped')]
        self.api.before_ref = complete_gate
        self.assertEqual(self.windows()['proceed'], 'true')
        self.assertFalse(self.api.cancelled())

    def test_completed_real_build_is_not_mistaken_for_skipped_gate(self):
        self.api.job_list = [dict(name='build', status='completed', steps=[
            dict(name='Claim Linux build', conclusion='success'),
            dict(name='Build kingdom.3dsx', conclusion='success')])]
        self.assertFalse(h.skipped_for_windows(self.api, 101, 1))

    def test_multiline_outputs_cannot_inject_new_output_keys(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder) / 'output'
            with patch.dict(os.environ, {'GITHUB_OUTPUT': str(p)}):
                h.emit(dict(proceed='true', release_name='hello\nproceed=false\n世界'))
            text = p.read_text(encoding='utf-8')
            self.assertIn('release_name<<handoff_', text)
            self.assertIn('hello\nproceed=false\n世界', text)
            self.assertEqual(text.count('<<handoff_'), 2)


class ReleaseRevisionTests(unittest.TestCase):
    def check_target(self, override):
        spec = importlib.util.spec_from_file_location('publish', Path(__file__).parents[1] / 'tools/publish_release.py')
        publish = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(publish)
        env = dict(GITHUB_REPOSITORY=REPO, GITHUB_SHA='b' * 40, RELEASE_NAME='latest')
        if override:
            env['RELEASE_SHA'] = SHA
        with patch.dict(os.environ, env, clear=True), patch.object(publish, 'Path') as path, patch.object(publish, 'run') as run:
            path.return_value.is_file.return_value = True
            path.return_value.stat.return_value.st_size = 123
            run.return_value.stdout = 'latest\n'
            publish.main()
        calls = [call.args for call in run.call_args_list]
        edit = next(args for args in calls if args[:3] == ('gh', 'release', 'edit'))
        update = next(args for args in calls if args[:2] == ('gh', 'api') and '--method' in args)
        expected = SHA if override else 'b' * 40
        self.assertEqual(edit[edit.index('--target') + 1], expected)
        self.assertIn('sha=' + expected, update)

    def test_handoff_release_targets_source_sha(self):
        self.check_target(True)

    def test_direct_release_still_uses_current_sha(self):
        self.check_target(False)


if __name__ == '__main__':
    unittest.main()
