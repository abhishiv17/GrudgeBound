# Owner hosting hand-off

Configuration prepared on 9 November 2026; **nothing deployed**. The owner reviews, commits and pushes,
then operates the host dashboards. Keep the chosen service running through 25 November. Only testnet is
authorized. No actual host URLs, secrets or private account configuration belong in this document.

## Before deployment

- T-14 code is in review. Verify forwarded-IP behavior on the actual ingress before enabling TRUST_PROXY.
- T-7 (agents' own accounts) is built and in review. Before hosting, the owner upgrades the items contract
  (code only) so agents can pay with `pay_minter`: command in contracts/README.md and the T-7 hand-off.
  The server creates the agent accounts on its first start with the chain on: `agent-fen.<gameAccount>`,
  `agent-vane.<gameAccount>`, `agent-silk.<gameAccount>`, `agent-rook-ash.<gameAccount>`,
  `agent-pell.<gameAccount>`, each with two function-call keys only. Their keys live in AGENT_STORE_PATH,
  encrypted with AUTH_SECRET: back the file up together with auth.enc. `GET /agents` lists the accounts.
- Resend's test sender can deliver only to the Resend account owner's address. Before judges sign in,
  the owner must add a domain, publish the requested DNS records, verify it and set AUTH_EMAIL_FROM to an
  address on that domain. [Resend's restriction and setup](https://resend.com/docs/knowledge-base/403-error-resend-dev-domain).
- Render Free sleeps and cannot attach this disk. Starter (0.5 CPU, 512 MB) is listed at **US$7/month**;
  persistent disks are **US$0.25/GB/month** (one GB here: nominal US$7.25/month before other usage/taxes).
  The owner accepts the charge and checks checkout/current pricing. Sources checked 9 Nov:
  [Render compute table](https://render.com/articles/set-up-self-hosted-github-actions-runners-on-render),
  [pricing](https://render.com/pricing), [disk restrictions](https://render.com/docs/disks).

## Chosen setup (10 Nov): Render Free + Upstash Redis

Render's paid plan needs a card the owner's bank would not accept, so render.yaml now uses the **free** plan
(no card, no disk). Free web services sleep after about 15 minutes without requests and lose their files on
every restart, so the server keeps its four data files in an **Upstash Redis** database (free tier, no card):

- Set `UPSTASH_REDIS_REST_URL` and `UPSTASH_REDIS_REST_TOKEN` (from the database's REST API panel).
  Point AUTH_STORE_PATH, SATCHEL_STORE_PATH, BATTLE_ARCHIVE and AGENT_STORE_PATH at `data/...` (no /var/data).
- On start the server copies auth.enc, satchels.json, agents.enc and battles.jsonl from Redis to disk
  (keys `gb:<file name>`); if Redis can't be read it refuses to start rather than start empty. Each later
  write is copied back (whole file, debounced; the battle archive appends line by line). SIGTERM flushes.
- auth.enc and agents.enc are uploaded exactly as on disk: AES-256-GCM ciphertext under AUTH_SECRET.
- The shared world (tick, Grudge, stock, Observatory history) still resets on each restart, as before.
- A sleeping service takes roughly 30-60 s to answer the first request. A free uptime monitor hitting
  `/health` every 5 minutes keeps it awake (750 free instance hours a month covers one service).
- Without the two UPSTASH variables nothing changes (local runs, tests, a paid disk).

The paid-plan steps below remain valid if a card becomes available later (then remove the UPSTASH variables
or keep them as a backup).

## Render game service

1. After committing reviewed source, create a Blueprint from the repository's main branch using render.yaml,
   or create a Node web service manually with repo root as its working directory. This is a long-running
   process, not a serverless function. Do not enable previews or horizontal scaling.
2. Select Starter, a one-GB disk at /var/data, build `npm ci`, start `npm run server`, health path /health.
   Keep automatic deploys off; deploy reviewed versions manually. The Blueprint includes these settings.
3. Enter the variables below directly in the dashboard. Every Blueprint variable has sync:false, so the
   Blueprint supplies no values. Render prompts only at initial creation; later variables must be added
   manually. [Blueprint reference](https://render.com/docs/blueprint-spec).

| Variable | Owner-entered value/purpose |
|---|---|
| NODE_VERSION | 24 (supports tsx's --env-file-if-exists); verify the selected host runtime |
| NODE_ENV | production |
| NPM_CONFIG_INCLUDE | dev — tsx is a devDependency required by npm run server, including at runtime |
| NEAR_NETWORK | testnet |
| NEAR_GAME_ACCOUNT, NEAR_ITEMS_CONTRACT, NEAR_RITE_CONTRACT, NEAR_RELAYER_ACCOUNT | Approved existing testnet account IDs, from private local configuration |
| NEAR_GAME_PRIVATE_KEY, NEAR_RELAYER_PRIVATE_KEY | Existing signer secrets; enter privately, never in repository files or build logs |
| NEAR_RPC_URL | Approved testnet RPC URL (omit to use code default) |
| AUTH_SECRET | New host-only 64-hex secret; preserve it with encrypted-store backups, never paste here |
| AUTH_KEY_MODE | server |
| AUTH_STORE_PATH | /var/data/auth.enc |
| SATCHEL_STORE_PATH | /var/data/satchels.json |
| BATTLE_ARCHIVE | /var/data/battles.jsonl |
| AUTH_SIGN_IN_URL | The final Vercel HTTPS origin plus /sign-in |
| AUTH_EMAIL_MODE | resend |
| RESEND_API_KEY, AUTH_EMAIL_FROM | Private Resend key and verified-domain sender |
| NEAR_AI_API_KEY, NEAR_AI_BASE_URL, NEAR_AI_MODEL | Existing approved configuration; API key optional for formula-only tooltips |
| TICK_MS, WORLD_SEED | Existing approved values (60000 and 2 by default); no retuning for hosting |
| TRUST_PROXY | 0 until ingress checks below pass; 1 then uses only the right-most valid XFF entry |
| RULE_PROPOSAL_COOLDOWN_MS | 60000 (OWNER default from T-14) |
| AGENT_CHAIN | on (off keeps the AI agents in the simulation only) |
| AGENT_STORE_PATH | /var/data/agents.enc |
| AGENT_DAILY_EMBER_CAP, AGENT_ACTIONS_PER_DAY | 200 and 6 (defaults; the agents' spend cap and action limit) |

Let the platform provide PORT. Do not set HOST to loopback; production already binds 0.0.0.0. Do not copy
local .env or ~/.near-credentials to the service. Supply signing keys through the dashboard secret fields.

4. Create the Vercel project below to obtain the sign-in origin; then set AUTH_SIGN_IN_URL and manually
   deploy the game service. Record its actual HTTPS URL privately and use it as Vercel GAME_SERVER_URL.
5. Complete the smoke checklist before replacing the README's pending host links.

## Vercel sign-in site

1. Import the same reviewed repository, framework Next.js, root directory **client**, Node 24. Enable
   access to files outside the root directory because client imports the sibling shared workspace.
   Use the repository workspace lockfile; install from the repository root with `npm ci --include=dev`
   (from client: `cd .. && npm ci --include=dev`); build from client with `npm run build`.
2. Set only GAME_SERVER_URL to the game service's HTTPS origin. Never put signer keys, AUTH_SECRET or
   RESEND_API_KEY in Vercel or any NEXT_PUBLIC_ variable. The browser uses /api/game on its own origin.
3. Deploy after owner review; use the stable production origin for AUTH_SIGN_IN_URL on the game service.
   Preview origins are refused by the game server's origin check. Redeploy Vercel after changing its env.
4. Open /sign-in and approve a native device session in the real browser; confirm the callback/session
   reaches Unreal. The sign-in page is the website's required role; this task adds no browser gameplay.

## Railway fallback

Use only if Render blocks the owner. Import repo root, use railway.json (Railpack, one replica, no sleeping),
and attach a volume at **/var/data** in the dashboard before starting. railway.json does not create a
volume or set secrets. Enter the same runtime variables above; set RAILPACK_NODE_VERSION=24 and retain
devDependencies because tsx is required. Disable automatic Git deployments and serverless sleeping in
the dashboard. Generate a public HTTPS domain, update Vercel GAME_SERVER_URL and the reciprocal sign-in
URL, then repeat smoke tests. [Railway config](https://docs.railway.com/config-as-code/reference).

## Persistence and restart behavior (read from source)

| Data written in server/ or agents/ | Persistent path control |
|---|---|
| Encrypted player identities, signer keys and session hashes; atomic .tmp sibling | AUTH_STORE_PATH |
| Satchel supplies, carried choices, ink and personal clock; atomic .tmp sibling | SATCHEL_STORE_PATH; defaults beside AUTH_STORE_PATH |
| Replay records and confirmed chain references | BATTLE_ARCHIVE |
| Development-only generated auth encryption key | AUTH_STORE_PATH + .key; production refuses missing AUTH_SECRET and never creates this file |

No other production runtime file writes were found under server/ or agents/. Native smoke/test helpers
write local temporary/test identity data and are not host startup commands. Pending agent persistence
is blocked with T-7. Back up the disk and AUTH_SECRET together through the owner's private backup process;
do not rotate/delete the secret without migrating the encrypted identity store.

**World tick/state does not survive a restart/redeploy.** index.ts calls createWorld(WORLD_SEED) and
createRng(WORLD_SEED). Ward Grudge, merchant stock/purses, simulated participants, Observatory history,
open battles, pending receipt queues and cooldowns reset. Active rules are reloaded from the Rite;
on-chain balances/ownership/vows persist. There is no world snapshot file/env variable to route to disk.
Avoid restarting during pending settlements: no durable queue reconciliation is implemented. This
single-process design must not be scaled to multiple replicas, even with disk storage.

## Smoke checks the owner runs on the real host

```powershell
$gameOrigin = 'https://OWNER_GAME_HOST'
Invoke-RestMethod "$gameOrigin/health"
Invoke-RestMethod "$gameOrigin/state"
Invoke-RestMethod "$gameOrigin/observatory?mode=live"
Invoke-RestMethod "$gameOrigin/observatory?mode=preview"
```

- /health returns ok:true without a session. /state advances after one configured tick; preview repeats
  the same history and does not advance live. No private key or email identity appears in these responses.
- Open the hosted /sign-in from Unreal's browser approval. Use two real email addresses, including one
  different from the Resend owner's address. Verify device poll completes, the game can battle/bind, L
  opens a testnet receipt, restart of Unreal remembers the session, and F10 revokes it.
- Check Satchel supplies/ink survive an owner-controlled service restart after all receipts settle;
  expect the shared world tick/history to reset. Verify the volume is still mounted before any redeploy.
- Check native sign-in from two networks. Send a spoofed left-most XFF and verify it cannot change the
  platform-added right-most client address. Render describes XFF but not a complete trusted-hop list:
  [Render IP guidance](https://render.com/articles/how-render-handles-ddos-attacks).
- **Code gap:** client/app/api/game/[...path]/route.ts deliberately drops proxy-address headers. Through
  Vercel, Render sees Vercel's outbound IP, so browser sign-in buckets may still be shared. TRUST_PROXY=1
  alone cannot fix the gateway. Correct preservation/authentication of that extra hop needs a separately
  scoped code change; T-9 cannot edit that route or index.ts. Do not claim the hosted limiter verified.
- `npm run native:smoke` does **not** accept a hosted URL. It spawns a loopback development server, reads
  console codes and uses data/native-smoke.enc. Do not run it as a hosted acceptance test or enable console
  email in production. `npm test` / `npm run typecheck` are local source checks; `npm run sim -- --json`
  previews the economy locally; `npm run replay <seed|battleId>` verifies an archived local battle.

## Point Unreal at the service

For an editor/local executable, pass `-GbServer=https://OWNER_GAME_HOST`. For a packaged build, supply
that URL to package-windows.ps1, or set `ServerUrl="https://OWNER_GAME_HOST"` under
[/Script/GrudgeboundClient.GbSettings] in unreal/GrudgeBound/Config/DefaultGame.ini for that build. Keep the
quotes: Unreal's ini reader treats an unquoted `//` as a comment and would read the URL as `https:`.
The command-line override still wins. Retain localhost in development; no hosted URL is fabricated here.

## Configuration validation

Both files can be checked against the hosts' current public JSON schemas. Render additionally offers
`render blueprints validate render.yaml` (CLI 2.7+) or its read-only validation API; the CLI is absent on
this laptop. Do not deploy to validate. [Render validation](https://render.com/docs/cli-reference).

Local validation: render.yaml passed Render's downloaded public JSON schema using Python jsonschema;
railway.json parsed successfully and its fields were checked against the published Railway schema/reference.
Downloading Railway's schema with urllib followed a redirect and received HTTP 403, so automated Railway
schema validation remains open. No authenticated host API or dashboard was used.
