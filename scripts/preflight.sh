#!/usr/bin/env bash
# Fast preflight: catch the failure classes that cost hours of CI time
# (shell syntax errors in derivation phases, eval errors on any system,
# broken workflow YAML) in seconds to minutes on a laptop.
# Run before every push that touches pkgs/, workflows, or actions:
#   scripts/preflight.sh
set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO_ROOT"

echo "== Nix syntax =="
for f in flake.nix pkgs/default.nix pkgs/*/default.nix; do
  nix-instantiate --parse "$f" > /dev/null || { echo "FAIL: $f" >&2; exit 2; }
done
echo "ok"

echo "== Workflow YAML (actionlint) =="
# Only our workflows: the inherited upstream LLVM workflows are not ours
# to fix and fail lint on their own terms.
if command -v actionlint > /dev/null 2>&1; then
  actionlint .github/workflows/patmos-ci.yml .github/workflows/patmos-ci-nix.yml
else
  nix run nixpkgs#actionlint -- \
    .github/workflows/patmos-ci.yml .github/workflows/patmos-ci-nix.yml
fi
echo "ok"

echo "== Shell syntax of every derivation phase (bash -n) =="
# One eval per output set (per-attr evals are far too slow on some
# machines); python drives bash -n over each phase string.
SYSTEM=$(nix eval --raw --impure --expr 'builtins.currentSystem' --accept-flake-config)
for set in packages checks; do
  nix eval --json ".$set.$SYSTEM" --accept-flake-config \
    --apply 's: builtins.mapAttrs (k: v: {
      configurePhase = v.configurePhase or null;
      buildPhase = v.buildPhase or null;
      checkPhase = v.checkPhase or null;
      installPhase = v.installPhase or null;
    }) s' > "/tmp/preflight-$set.json"
  python3 - "$set" <<'EOF'
import json, subprocess, sys
set_name = sys.argv[1]
data = json.load(open(f"/tmp/preflight-{set_name}.json"))
failures = []
for attr, phases in data.items():
    for phase, script in phases.items():
        if not script:
            continue
        proc = subprocess.run(["bash", "-n"], input=script,
                              text=True, capture_output=True)
        if proc.returncode != 0:
            print(f"FAIL: {set_name}.{attr}.{phase}:\n{proc.stderr}", file=sys.stderr)
            failures.append(attr)
sys.exit(2 if failures else 0)
EOF
done
echo "ok"

echo "== Evaluate every flake output, no build =="
nix flake check --no-build --show-trace --accept-flake-config
echo "ok"

echo "Preflight passed."
