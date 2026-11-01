# Contracts

Two Rust contracts for NEAR (testnet during the hackathon).

| Contract | Account (planned) | What it holds |
|---|---|---|
| `items/` | `items.grudgebound.testnet` | Ember coin + every talisman in one multi-token contract (NEP-245 shape; compliance is WIP) |
| `rite/` | `rite.grudgebound.testnet` | Points, rules (propose / resolve / repeal), vows (stake / settle), battle results |

**Status (Day 1):** skeletons with the core bookkeeping written. They have **not been compiled yet**:
the first job for the Chain & Server seat is to install the toolchain, build them and fix what the compiler says.

## Setup (Chain & Server seat)

Use **WSL2 (Ubuntu)** on Windows; Rust contract builds are much smoother there.

Get the exact, current commands from **https://docs.near.org (Smart Contracts > Quickstart)**.
At the time of writing the steps are roughly:

1. Install Rust with `rustup`, then add the WebAssembly target (`rustup target add wasm32-unknown-unknown`).
   Use the Rust version the NEAR docs recommend.
2. Install `cargo-near` (build tool) and NEAR CLI.
3. Create the testnet accounts (`grudgebound.testnet` plus `items.` and `rite.` sub-accounts).
4. Build each contract from its folder with `cargo near build`, then deploy and call `new(...)`.

Never commit keys. NEAR CLI stores credentials outside the repo; `.gitignore` also blocks common key files.

## TODOs (from the code)

- `rite`: validate rule templates and parameters on-chain (mirror `shared/src/rules.ts`).
- `rite`: apply money rules (R1, R4, R5) to battle results here instead of trusting the server's numbers.
- `rite`: let vows be settled as kept by anyone after they end; require evidence for breaches.
- `items`: finish NEP-245 (approvals, `mt_batch_transfer`, `mt_transfer_call` + resolve, metadata views).
- Both: unit tests with `near-sdk`'s `unit-testing` feature, and a testnet deploy script.
