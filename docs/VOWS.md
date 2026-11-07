# Unreal Vow Shrine · Phase 3

The player opens the native Shrine with **V**, chooses a promise, its duration and any parameter, and stakes
points. The game server relays `make_vow` for the authenticated player. The Rite locks the points and stores
the deadline. The Shrine shows the promised return, the active vows, history and time remaining; **L** opens
the last transaction's proof. No browser gameplay screen is involved.

| Promise | Stake | Town days | Enforced restriction |
|---|---:|---:|---|
| V1: stay in a Ward | 20 | 1–3 | Encounters and merchant trades outside the promised Ward are refused |
| V2: no healing items | 15 | 1–2 | Salve is disabled; a forged item action is refused before spending a turn |
| V3: keep a talisman | 10 | 3 | The selected owned token cannot be sold |
| V4: fight only one omen | 25 | 1 | Other omens allow fleeing; attack, ward and seal are disabled/refused |

Restricted actions do not silently burn the stake. Return to the Shrine and select **Release selected vow
(burn its stake)** to renounce the promise. Changing vows ends any outstanding encounter for that account,
including one previously slipped away from; it cannot be resumed under different restrictions.

One town day is `24 * TICK_MS` milliseconds (24 real minutes with the default tick). The duration selected at
swearing becomes the contract's absolute deadline. Changing the tick speed or restarting Unreal/server does
not reset it. The server periodically checks known signed-in accounts and settles expired vows as kept;
temporary RPC failures retry. Only the server chooses `kept`; the public API accepts no client verdict.

A kept vow returns its stake plus the contract's integer **20%** bonus. An early release burns the stake and
adds four Dread in its Ward (Lantern if the vow has no Ward parameter). The Shrine does not advertise the
planned attack/binding/level/purse bonuses: those and the one-day Oathbreaker status still need integration.

The current sign-in configuration must support testnet signing for new email identities. See `SIGN_IN.md`.
With `NEAR_CHAIN=off`, the panel displays its closed state; no fake chain stakes are awarded.

## Server interface

All these routes require the signed-in session; the server supplies the acting account.

| Route | Body / result |
|---|---|
| `GET /vows` | Fixed template stakes, allowed durations, owned tokens, points, history and contract deadlines |
| `POST /vows/swear` | `{templateId, days, params}`; returns `{jobId, chain}` |
| `POST /vows/release` | `{vowId}` owned by this player; expired vows cannot be burned |
| `GET /chain?vow=<jobId>` | Receipt restricted to its owner |

The existing `rite` contract needs no redeploy for this integration. The backend uses its `get_vows`,
`make_vow` and `settle_vow` methods and the same transaction queue as battle rewards and trading.

## Verification · 7 Nov

- `npm test`: **108/108**. Added checks for fixed stake/duration, parameter and token ownership validation,
  concurrent requests, deadline settlement after restart, early burn, retry after write failure, account
  isolation and battle/trade restrictions. HTTP checks cover missing sessions and impersonation.
- `npm run typecheck`: passes. Unreal Development Editor target: builds successfully on UE 5.8.4.
- Live testnet: `npm run vow:smoke -- afreen.grudgebound.testnet` passed using the existing Phase 2 Unreal
  demo account. No new accounts or keys were created. Points moved **70 → 55 → 73 → 58**: first stake,
  kept return, then a second stake released/burned.
- Proof: [first stake](https://testnet.nearblocks.io/txns/5qiDsX93JTDaXDi2j64Xx7fWnAaKxzX9GLc6vmppCR3X),
  [second stake](https://testnet.nearblocks.io/txns/FW6xAeqLjQN1DkT1EJjVdr6g39bubteVCvpvSYvKmPfg),
  [release/burn](https://testnet.nearblocks.io/txns/7H8B2pz6bACszqBBXanEJQbFXf8yPvYGDyC5kTeBtrZC).

The smoke command deliberately stakes and burns **testnet points** and must only be run on a chosen demo
identity. It refuses mainnet, missing signers, insufficient earned points and an existing active V2 vow.

Remaining native acceptance:

- [ ] Sign in as a testnet player, press V, swear a vow and see the stake deducted in Shrine and HUD.
- [ ] Verify V2 disables Salve and V3 refuses selling the promised talisman in the Broker.
- [ ] Release a vow; see Broken, the stake burned, and L open the settlement proof.
- [ ] Keep a short vow with `TICK_MS=1000`, restart Unreal/server, then see the returned stake and bonus.
- [ ] Sign out and use another identity; the Shrine must show only that player's vows and tokens.
- [ ] Check panel layout at the packaged game's target resolution.
