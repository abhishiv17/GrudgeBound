# Player sign-in · first build

P3-C1, P3-G1 and P3-U1: email codes, persistent identities, revocable seven-day sessions, and a browser handoff
to Unreal. The existing contracts and relayer settle battles, purchases and rules for the authenticated account.
The backend rejects account impersonation and turns submitted for another player's battle.

## Try the flow locally without sending email or creating accounts

In a PowerShell terminal at the repository root:

```powershell
$env:NEAR_CHAIN = 'off'
$env:AUTH_EMAIL_MODE = 'console'
npm run server
```

In another terminal, run `npm run dev`. Open `http://localhost:3000/sign-in`, enter an email, and copy the
six-digit code from the **server terminal**. This mode is a local development aid: it does not verify access
to a real mailbox. It is forbidden in production, and the game server must bind only to loopback.

In Unreal, press Play and select **Sign in with email**. Complete the browser page, then return to the game.
The gate closes automatically. F10 signs out while in the Ward. Returning to Play resumes the saved session.
No player account setting is needed in Project Settings.

Local identity records persist in `data/auth.enc`. Without `AUTH_SECRET`, development creates its encryption
key in `data/auth.enc.key`. Both files are ignored by git. Keep them together to preserve local identities.
Off-chain development saves are per player on the client; only on-chain points and inventory survive across devices.

## Real email and testnet

Configure the root `.env` for the game server:

```dotenv
AUTH_EMAIL_MODE=resend
RESEND_API_KEY=<server-only sending key>
AUTH_EMAIL_FROM=Hollowmere <signin@your-verified-domain>
AUTH_SIGN_IN_URL=http://localhost:3000/sign-in
AUTH_SECRET=<64 random hexadecimal characters>
```

Use `node -e "console.log(require('node:crypto').randomBytes(32).toString('hex'))"` to generate a secret locally.
The Resend sender/domain must be verified. The implementation uses the [send-email API](https://resend.com/docs/api-reference/emails/send-email).
No real email was sent during automated verification.

**Key custody is still a product decision.** The roadmap originally specified player-held keys. This build
contains an explicit, optional `AUTH_KEY_MODE=server` implementation for the testnet demo; it stays disabled
unless configured. It creates one full-access Ed25519 key per player and encrypts it in the auth store.
That is server custody, not player-held signing. It is rejected on mainnet. Player-held signing/recovery still
needs its own integration if that option is chosen. No private key is returned to either client.

With the server-held option explicitly enabled and the existing testnet master/relayer configured, the first
verified login prepares `p<random player id>.<game account>` with 0.05 NEAR from the game master. Subsequent
logins reuse that account and key. The key and account name are saved before creation; retries check for the
same access key rather than funding a second account. Account creation shares the chain writer's transaction
queue. Real provisioning, minting and trading for a newly signed-in player still need a testnet/editor check.

The web gateway defaults to `http://localhost:8787`. For a different game-server address, set `GAME_SERVER_URL`
in **client/.env.local** or the web host's environment, then restart Next.js. Next.js runs from `client/` and
does not load the root server `.env` automatically. The browser always calls `/api/game` on its own origin.
Use the exact web origin configured in `AUTH_SIGN_IN_URL` (`localhost` and `127.0.0.1` are different origins).

## Deployment requirements

- Set `NODE_ENV=production`, an HTTPS `AUTH_SIGN_IN_URL`, `AUTH_SECRET`, a real sender, and the selected key mode.
- Keep one game-server process and a persistent, private volume for `AUTH_STORE_PATH`; back up it and its secret.
  This file store is not a multi-process database. Production never generates an encryption secret automatically.
- Serve the Next.js gateway over HTTPS. Browser sessions use an HttpOnly, Secure, SameSite=Strict cookie.
  Never put sender credentials or `AUTH_SECRET` in a `NEXT_PUBLIC_*` variable.
- Unreal keeps its session in Windows DPAPI storage bound to the current Windows user and game-server URL.
  Other platforms keep it in memory. Player keys are never packaged with the game.
- Email codes expire after ten minutes and five wrong guesses. Requests are limited by email and socket IP;
  behind the Next.js proxy, the IP limit is shared. Configure edge rate limiting before wider deployment.
- Pending codes, device approvals and rate limits are in memory and reset when the server restarts. Issued
  sessions persist. The game retries sign-in after an interrupted or expired approval.

## API and handoff

| Route | Purpose |
|---|---|
| `POST /auth/email/start` | `{email, device?}` → challenge ID; sends a code, never returns it |
| `POST /auth/email/verify` | `{challengeId, code}` → browser cookie, or approves the pending game sign-in |
| `POST /auth/device/start` | Creates a private device code and a separate browser approval URL |
| `POST /auth/device/poll` | `{deviceCode}` → pending, or a one-time native session token and player |
| `GET /auth/session` | Current authenticated player; excludes signing keys |
| `POST /auth/logout` | Revokes that session and clears the browser cookie |

The native handoff uses separate approval and polling secrets, following the separation in the
[device authorization flow](https://www.rfc-editor.org/rfc/rfc8628.html). It is an application-specific flow,
not an OAuth authorization server. Unreal polls at two-second intervals for at most ten minutes; it does not
run a local HTTP listener. The browser requires an explicit sign-in submission before approving the game.
Only the private device code can collect the session. It never appears in the browser URL.

Protected routes accept `Authorization: Bearer <token>` for Unreal or the browser cookie. `POST /battle/start`,
`/battle/turn`, `/shop/buy`, `/shop/sell`, `/rules/propose`, `/rules/repeal`, and `GET /player` require sign-in.
The server supplies the acting account. Public town state, catalogues and chain proof links remain readable.

## Verification

Automated checkpoint (7 Nov): **100/100 tests**, workspace type checks, the Next.js production build, and the
Unreal Development Editor target pass. A native handoff is reserved while provisioning its player, so two
concurrent email approvals cannot replace each other's account. The regression test holds provisioning open
and verifies that the second approval is refused. This does not complete the manual checks below.

Manual local checks (reported by the user, 6 Nov):

- [x] Browser email-code sign-in succeeds using console delivery.
- [x] Unreal opens browser sign-in, receives the approved session, and returns to gameplay.
- [x] Bound talisman and points survive stopping and restarting Play (reported by the user).
- [ ] Session and progress survive restarting Unreal and the game server. User reported entering the Ward with apparently missing points/talismans. Inspection confirmed that the server accepts the saved session and the per-player file retains 5 points and two Cinder Grin talismans (starter plus the test bind), matching the one archived bind worth 5 points. The separate legacy `Grudgebound` save retains 70 points and six talismans. Confirm the post-restart HUD and Case display the per-player values before ticking this check.
- [ ] F10 signs out; two email identities retain separate progress.
- [ ] Real email delivery and new-player testnet provisioning, binding, trading and rule proposal pass.

`npm test` includes service tests for codes, limits, retries, persistence, encrypted storage, native approval,
logout and impersonation, plus an HTTP test that starts the real server with no chain or mail dependencies.
Run `npm run typecheck`, `npm run build`, and the Unreal editor-target build too. The remaining acceptance check
is real-mail sign-in from Unreal followed by a testnet bind/trade, restart/resume, and logout.
