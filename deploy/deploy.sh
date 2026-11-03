#!/usr/bin/env bash
# Runs on the VPS from a release directory staged by GitHub Actions.
set -euo pipefail
release=$(cd -- "$(dirname -- "$0")" && pwd -P)
root=$(cd -- "$release/../.." && pwd -P)
exec 9>"$root/deploy.lock"
flock 9

# CD is private and compile-only. The host reverse proxy is configured separately.
export KMY_WEB_BIND=127.0.0.1 KMY_WEB_PORT=8088 KMY_EXECUTION_MODE=compile
unset COMPOSE_PROFILES KMY_WEB_IMAGE KMY_BACKEND_IMAGE KMY_RUNNER_IMAGE KMY_JOB_IMAGE
previous=$(readlink -f "$root/current" || true)

compose() {
    local directory=$1
    shift
    docker compose --project-name kmy-playground --env-file "$directory/images.env" \
        --file "$directory/compose.yaml" "$@"
}

compose "$release" config --quiet
compose "$release" --profile build pull web backend runner job-image
if compose "$release" up -d --no-build --pull never --force-recreate --wait --wait-timeout 30 && \
   python3 "$release/smoke.py"; then
    ln -sfn -- "$release" "$root/current"
    echo "Deployed $release"
else
    compose "$release" logs --tail=50 || true
    if [[ -n "$previous" && "$previous" != "$release" && -f "$previous/images.env" ]]; then
        echo "Deployment failed; restoring $previous" >&2
        compose "$previous" up -d --no-build --pull never --force-recreate --wait --wait-timeout 30
        python3 "$previous/smoke.py"
    elif [[ ! -L "$root/current" ]]; then
        compose "$release" down
    fi
    exit 1
fi
