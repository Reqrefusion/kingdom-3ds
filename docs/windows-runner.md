# Windows runner and queued Linux handoff

The existing **Build 3DSX** workflow keeps its push, pull request and manual
triggers, Linux container, build commands, artifact and release behavior.
**Build 3DSX (Windows)** is an additional workflow. A `workflow_run: requested`
event queues it on `self-hosted, Windows, X64, kingdom-3ds-local`.

## Automatic behavior

- Windows only takes a same-repository push or manual run whose Linux `build`
  job is still queued (including workflow-concurrency queues with no job yet).
- Linux and Windows create the same immutable ownership ref before compilation:
  `refs/tags/kingdom-runner-claims/<source-run-id>-<source-attempt>`. The annotated
  tag records the winning runner and source revision. Only one create succeeds.
  No source commit is created. Claims made with `GITHUB_TOKEN` do not trigger
  recursive push workflows.
- Windows claims first, cancels the original run, waits for cancellation, checks
  out its exact source SHA, builds and uploads `kingdom.3dsx`. Default-branch,
  tag and manual runs retain their original release names/publication policy.
- If Linux has already started its job, Windows skips. If Linux starts checkout
  or tool installation during the queue-check race, the shared gate stops it
  before compilation. If Linux claims first, Windows never cancels it.
- Pull requests, forks and runs created before this protocol stay on Linux.
  Windows can still be started directly with Run workflow and a release tag.
- With the PC offline, Linux continues normally. The unused Windows request may
  remain queued until the runner returns or GitHub expires it; on starting it
  checks again and skips if the original is no longer waiting.

## Setup

Required on the runner user's PATH: Git, GitHub CLI, Python 3.9+ with Pillow,
and FFmpeg. Install devkitPro/devkitARM, libctru, citro2d, citro3d and the 3DS
tools. DEVKITPRO defaults to C:/devkitPro. No Docker or WSL is needed.

Download the official Windows x64 runner from
https://github.com/actions/runner/releases into C:/workspace/kingdom-3ds-runner.
Verify its SHA-256 against the release asset's digest before extraction.
Then run in PowerShell as the Windows user who owns the build tools:

```powershell
gh auth login --hostname github.com --git-protocol https --web --scopes repo,workflow
powershell -NoProfile -ExecutionPolicy Bypass -File tools/register_windows_runner.ps1
```

Registration requires repository admin access. The registration token is fetched
from GitHub and is not written to a script. The launcher starts hidden and uses
a mutex to prevent duplicate listeners. HKCU Run starts it at Windows sign-in;
it is not a system service and cannot work before sign-in or while asleep.
The runner folder contains credentials: keep it private and out of git.
Diagnostics are in its _diag directory and listener.log.

Both workflows must be present on the default branch to enable automatic
handoff. A local edit alone does not change GitHub Actions. The source run title
carries JSON with the full ref and manual release input; do not change that
format without updating runner_handoff.py. Source attempts are isolated.
GitHub does not send the `requested` event on reruns: reruns use Linux normally.

The build job needs contents:write to atomically create its claim tag; the
Windows build additionally needs actions:write to cancel the claimed source.
Repository tag rules must permit creation under kingdom-runner-claims/.
Fork PR tokens remain read-only and PRs bypass the claim API entirely.
Claims must remain immutable while a run/attempt can still execute; they may be
removed after all related runs have completed, but are retained by default.

## Failure and verification

Cancellation errors, a changed source attempt, or an unconfirmed cancellation
stop the Windows handoff rather than permit a duplicate build. If a failure
occurs after Windows has claimed/cancelled Linux (including a Windows build
failure or the PC sleeping), use **Re-run all jobs** on the original Linux run.
This creates a new claim attempt; it does not automatically retry the failed
Windows takeover. Avoid manually rerunning/cancelling the original during an
active handoff, since GitHub's cancellation endpoint targets a run ID and does
not offer an atomic attempt-specific cancellation condition.

Local verification:

```powershell
python -m unittest discover -s tests -p test_runner_handoff.py -v
python tools/port.py check --quick
```

Once these changes are on GitHub, verify: a queued trusted run is replaced by
Windows at the same SHA; a started Linux run is left alone; a fork PR never
runs on the PC; the release points at the source SHA rather than the Windows
workflow's default-branch SHA. Check Settings > Actions > Runners for the
runner's online status. Windows claims can turn an original Linux status check
into cancelled/skipped; branch-protection rules requiring that exact Linux
check may need the Windows result to be considered separately.

To disable automatic start, remove Kingdom3DSRunner under
HKCU/Software/Microsoft/Windows/CurrentVersion/Run. Stop the runner before
removing its registration in Settings > Actions > Runners.
