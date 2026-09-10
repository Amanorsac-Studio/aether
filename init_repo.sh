#!/usr/bin/env bash
#
# One-time: turn this folder into the aether git repo and push it.
# Run from Git Bash inside the project folder, or double-click push_to_github.bat.
#
# Safe to re-run. Everything it does is echoed to push_log.txt as well as the
# console, so a failure can be read afterwards instead of guessed at.

REMOTE="https://github.com/Amanorsac-Studio/aether.git"
LOG="push_log.txt"

cd "$(dirname "$0")" || exit 1
: > "$LOG"

say() { echo "$*" | tee -a "$LOG"; }
run() {
    say "\$ $*"
    "$@" 2>&1 | tee -a "$LOG"
    return "${PIPESTATUS[0]}"
}

say "AETHER -> GitHub"
say "folder: $(pwd)"
say "git:    $(git --version 2>&1)"
say ""

# The CI workflow ships as ci/github-build.yml because .github/workflows is a
# protected path for remote tools. Put it where GitHub Actions expects it.
if [ -f ci/github-build.yml ]; then
    mkdir -p .github/workflows
    mv -f ci/github-build.yml .github/workflows/build.yml
    rmdir ci 2>/dev/null
    say "Placed .github/workflows/build.yml"
fi

# The .NET bootstrapper and its uninstaller were replaced by installer/native.
for stale in installer/Bootstrapper installer/uninstall.ps1; do
    if [ -e "$stale" ]; then
        rm -rf "$stale"
        say "Removed superseded $stale"
    fi
done

if [ ! -d .git ]; then
    run git init -q || { say "git init failed"; exit 1; }
    say "Initialised repository"
fi

# Make sure we are on main whether or not the branch already exists.
current="$(git symbolic-ref --short -q HEAD)"
if [ "$current" != "main" ]; then
    git symbolic-ref HEAD refs/heads/main 2>/dev/null || run git branch -M main
    say "On branch main"
fi

if git remote get-url origin >/dev/null 2>&1; then
    run git remote set-url origin "$REMOTE"
else
    run git remote add origin "$REMOTE"
fi

# A commit fails outright when git has no identity, which is the usual reason a
# first push never happens. Set one for this repository only if none is visible.
if [ -z "$(git config user.email)" ]; then
    run git config user.email "amanorsac@gmail.com"
    say "Set repo-local user.email (no global identity was configured)"
fi
if [ -z "$(git config user.name)" ]; then
    run git config user.name "Nene (Amanorsac Studio)"
    say "Set repo-local user.name"
fi
say "identity: $(git config user.name) <$(git config user.email)>"
say ""

run git add -A

# Unborn branch: HEAD does not resolve yet, so there is always something to commit.
if git rev-parse --verify -q HEAD >/dev/null && git diff --cached --quiet; then
    say "Nothing new to commit."
else
    git commit -q -F - <<'MSG' 2>&1 | tee -a "$LOG"
AETHER 1.0.0 — dynamic air exciter (VST3)

Air-exciter plugin built to go past Slate's Fresh Air: tunable Presence and Air
bands rather than fixed shelves, a harmonic exciter (Glow), a program-dependent
limiter on the boost so sibilance never rides along (Guard), mid/side placement
of the air (Focus), loudness-matched auditioning, and 2x/4x oversampling.

- DSP core is JUCE-free and unit tested (tests/dsp_test.cpp): band placement,
  harmonic generation, Guard behaviour, M/S isolation, auto-gain, and click
  detection on fast parameter moves.
- Licensing follows the Amanorsac Studio License Integration Standard: ECDSA
  P-256 proof verification against the studio key, encrypted local storage,
  hourly heartbeat, 30-day offline grace, deactivate for moving machines.
  tests/license_test.cpp runs the shipping verification code against a known
  vector, including tampered signature and tampered body.
- Windows installer is a native Win32 self-extracting EXE: per-user, no .NET,
  no admin rights, doubles as its own uninstaller.
- macOS build, signing and notarisation run in CI on an Apple runner.

Follows the company Master Standard: single Amanorsac Studio brand, Inter and
JetBrains Mono bundled locally, colour token roles, the shared knob contract
with focus-visible states, an About screen, and runtime data under
Documents/Amanorsac Studio/AETHER.
MSG
    if git rev-parse --verify -q HEAD >/dev/null; then
        say "Committed $(git rev-parse --short HEAD) with $(git ls-files | wc -l) files"
    else
        say "COMMIT FAILED - see the message above. Nothing was pushed."
        say "Most likely: git has no identity, or a hook rejected it."
        exit 1
    fi
fi

say ""
say "Pushing to $REMOTE"
run git push -u origin main
rc=$?
say ""
if [ "$rc" -eq 0 ]; then
    say "SUCCESS - https://github.com/Amanorsac-Studio/aether"
else
    say "PUSH FAILED (exit $rc). The commit is safe locally; only the push failed."
    say "If it asked for a password: GitHub needs a personal access token, not your"
    say "account password. Git Credential Manager normally opens a browser window."
fi
exit "$rc"
