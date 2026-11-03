# GitHub → GHCR → VPS

GitHub `main` is the source of truth for the app, the instrumented compiler snapshot, and the deployment configuration. The VPS runs the exact image digests from a tested GitHub build; it does not compile the repository or run a GitHub Actions agent. Local commits do nothing until pushed. Do not edit deployed release files by hand.

## What the workflow does

1. **Pull requests:** build the Linux compiler from `compiler/` in Ubuntu 22.04, run compiler and runner regressions, build React, test and package Spring, then build the four images. Smoke-test compile-only mode through Nginx and check native execution and browser replay logic with trusted fixtures. Stop the test stack afterward. PRs do not publish or deploy.
2. **Push/merge to `main`:** after the same checks, publish `kmycvisualiser-{web,backend,runner,job}` to `ghcr.io` using GitHub's workflow token. Tag with the commit SHA and generate `images.env` with immutable image digests. The workflow artifact includes that manifest, Compose, the deploy script, and the smoke test.
3. **Deploy, if enabled:** serialize production jobs and skip a release if its commit is no longer the tip of `main`. SSH copies the release to `~/kmy-deploy/releases/`. The VPS pulls all four images, starts web/API/dispatcher with `--no-build`, and tests the real compile-and-replay endpoint. The job image is pulled but only runs on submissions. After success, `~/kmy-deploy/current` points to the new release. A failed startup or smoke test attempts to restore the prior release.

This is a single-server rollout, so service restarts can briefly interrupt requests. Keep the current and previous release images available for rollback. Docker images and release directories accumulate over time; monitor disk space and clean up older versions deliberately.

## One-time setup

1. On the VPS, install Docker Engine with the Compose plugin, Python 3, and `flock` (usually provided by `util-linux`). Verify `docker compose version` and `docker info` as the deployment account. Make sure loopback port `8088` is free. This deployment uses the fixed Compose project name `kmy-playground`; don't run a second manual stack on that port.
2. Create a **dedicated deployment SSH key** and authorize its public key for the VPS deployment account. Don't reuse `gigaplus`, paste a private key into chat, or commit it. The account needs Docker access; a user with the standard Docker socket is effectively a VPS administrator. Treat the deploy key accordingly. Get the server's SSH host public key via the provider console or an independently verified connection and format it as a `known_hosts` entry matching the deploy hostname. The workflow refuses unknown or changed host keys.
3. In GitHub repository **Settings → Environments**, create `production`. Limit deployments to `main` if available. Add environment variables `VPS_HOST` and `VPS_USER`, and environment secrets `VPS_SSH_KEY` (the dedicated private key) and `VPS_KNOWN_HOSTS` (the verified host entry). SSH uses port 22. Protect `main` with required reviews/checks if available.
4. Push the workflow and source to `main` with deployment still disabled. Verify the workflow passes and publishes all four packages. On the VPS, if the GHCR packages are private, log in to `ghcr.io` **as the deploy account** using a GitHub account with access and a classic token scoped `read:packages`. Use `docker login ghcr.io -u YOUR_GITHUB_USER --password-stdin` with the token supplied via stdin, never as a command-line argument. GitHub's short-lived workflow token is used only for publishing in Actions.
5. Set **repository** variable `DEPLOY_ENABLED=true` under **Settings → Secrets and variables → Actions → Variables**. Run the workflow manually on `main` or push another commit. This switch gates deploy only, not build or publishing.

Auto-deploy binds to `127.0.0.1:8088` and forces compile-only mode. To inspect from your computer, forward `localhost:8088` to the VPS's `127.0.0.1:8088` with SSH (`-L 8088:127.0.0.1:8088`) and open `http://localhost:8088`. For a public site, put an HTTPS reverse proxy in front after testing. This does **not** add the file-access sandbox required for public native execution.

### Public `/kmy/` on the personal site VPS

The personal site's Nginx still owns port 80. The playground and personal-site web containers share the external `kmy-public-edge` network (`172.30.88.0/28`); the personal proxy uses `172.30.88.2`. The deploy script automatically attaches only the playground web container when the network exists. The personal-site files in [`personal-site/`](personal-site/) are installed alongside its existing `compose.yaml`. This keeps the home page at `/`, proxies `/kmy/` to the playground, and leaves Spring and the runner unexposed. This public IP route uses HTTP; use HTTPS before asking visitors to submit private code.

To watch completed HTTP submissions (no source code or query string is logged):

```sh
docker logs --tail 50 -f personal-site-web-1 2>&1 | grep --line-buffered 'kmy_submit '
```

HTTP `200` means the compiler stream was delivered, not necessarily that the submitted source compiled successfully. The browser shows the actual compiler result.

## Inspect and roll back

On the VPS, inspect the active version with `readlink ~/kmy-deploy/current`. To roll back, disable `DEPLOY_ENABLED`, wait for any running deploy to finish, and run `bash ~/kmy-deploy/releases/<older-release>/deploy.sh` on a previously successful release. The same pull, smoke test, and rollback logic applies. Then revert or fix the bad commit on `main` before re-enabling deployment.
