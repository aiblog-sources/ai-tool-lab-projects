#!/usr/bin/env bash
# Recreate fixture/package-lock.json. The lockfile used in the runs is not committed: the projects repo's
# push guard reads version strings that start with "10." as private IP addresses. npm resolves the newest versions
# that match package.json, so a lockfile made later can differ; the one measured (2026-10-21) had
# sha256 9b382b2af7cf89db210939ebf7a59e2e8c8d7b62c61f3d12cb8c161b61e3a0ba, 288 packages, express 4.22.3,
# debug 4.4.3 + 2.6.9, cookie 0.7.2, vite 5.4.21. Edit bench/questions.json if yours differs.
set -euo pipefail
cd "$(dirname "$0")/fixture"
npm install --package-lock-only --ignore-scripts --no-audit --no-fund
sha256sum package-lock.json
