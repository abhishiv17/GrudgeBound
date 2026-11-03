# Contracts

Two Rust contracts for NEAR (testnet during the hackathon).

| Contract | Account (planned) | What it holds |
|---|---|---|
| `items/` | `items.grudgebound.testnet` | Ember coin + every talisman in one multi-token contract (NEP-245 shape; compliance is WIP) |
| `rite/` | `rite.grudgebound.testnet` | Points, rules (propose / resolve / repeal), vows (stake / settle), battle results |

**Status (1 Nov):** both contracts **compile to wasm** (`items` 324 KB, `rite` 374 KB, Rust 1.99, near-sdk 5.30).
Unit tests are written for both (7 for `items`, 10 for `rite`) using near-sdk's mocked blockchain.

## Build (any OS)

```sh
rustup target add wasm32-unknown-unknown
cd contracts
cargo build --target wasm32-unknown-unknown --release
# -> target/wasm32-unknown-unknown/release/grudgebound_items.wasm and grudgebound_rite.wasm
```

On Windows without admin rights, the GNU toolchain works for this build:
`rustup-init -y --default-host x86_64-pc-windows-gnu -t wasm32-unknown-unknown`, then add `%USERPROFILE%\.cargo\bin` to PATH.

## Test

```sh
cd contracts
cargo test
```

- **Linux, macOS, WSL2 or Windows with the MSVC toolchain:** runs as is. All 17 pass (3 Nov, Windows 11, Rust 1.99.1 MSVC,
  Visual Studio 2026 C++ tools). `.cargo/config.toml` sets `AWS_LC_SYS_PREBUILT_NASM=1`, so no NASM install is needed.
  MSVC install: `rustup-init -y --default-host x86_64-pc-windows-msvc -t wasm32-unknown-unknown` (needs the VS C++ workload).
- **Windows with the GNU toolchain:** the tests' dependencies need GNU `dlltool` + `as`, which that toolchain does not ship,
  so use the MSVC toolchain above, or Docker (Docker Desktop must be running):
  ```sh
  docker run --rm -v "%cd%":/c -w /c rust:1 sh -c "cargo test"
  ```

## Deploy (testnet, live since 4 Nov)

| Account | Role | Funded with |
|---|---|---|
| `grudgebound.testnet` | Game master: `items` minter, `rite` game_server and arbiter (Kessa) | 10 NEAR from the faucet |
| `items.grudgebound.testnet` | `items` contract | 2.5 NEAR |
| `rite.grudgebound.testnet` | `rite` contract | 2.5 NEAR |
| `relayer.grudgebound.testnet` | Pays players' gas (P2-C3) | 2 NEAR |

Look them up on https://testnet.nearblocks.io (e.g. `/address/items.grudgebound.testnet`).
Keys live in `%USERPROFILE%\.near-credentials\testnet` on the laptop that created them, never in the repo.

Tools (prebuilt installers, Windows PowerShell):

```powershell
irm https://github.com/near/near-cli-rs/releases/latest/download/near-cli-rs-installer.ps1 | iex   # near 0.30.1
irm https://github.com/near/cargo-near/releases/latest/download/cargo-near-installer.ps1 | iex     # cargo-near 0.22.0
```

Build. On Windows add `--no-abi`: the ABI step links the contract as a native DLL and MSVC rejects the
NEAR host functions (LNK1120). The wasm is the same without it.

```sh
cd contracts/items && cargo near build non-reproducible-wasm --no-abi   # -> target/near/grudgebound_items/*.wasm (154 KB)
cd contracts/rite  && cargo near build non-reproducible-wasm --no-abi   # -> target/near/grudgebound_rite/*.wasm (171 KB)
```

First deploy (already done; `new` can only run once per account):

```sh
near contract deploy items.grudgebound.testnet use-file target/near/grudgebound_items/grudgebound_items.wasm \
  with-init-call new json-args '{"minter":"grudgebound.testnet"}' prepaid-gas '100.0 Tgas' attached-deposit '0 NEAR' \
  network-config testnet sign-with-legacy-keychain send
near contract deploy rite.grudgebound.testnet use-file target/near/grudgebound_rite/grudgebound_rite.wasm \
  with-init-call new json-args '{"game_server":"grudgebound.testnet","arbiter":"grudgebound.testnet"}' \
  prepaid-gas '100.0 Tgas' attached-deposit '0 NEAR' network-config testnet sign-with-legacy-keychain send
```

Upgrade the code later: same command with `without-init-call` instead of `with-init-call ... attached-deposit '0 NEAR'`.
If the stored state layout changed, add a migration first (or deploy to a fresh account).

Smoke test (4 Nov): `mint_talisman` to the relayer returned `tal-1` and `talisman {"token_id":"tal-1"}` reads it back;
`submit_battle` with +5 points, then `get_points` returns 5.

```sh
near contract call-function as-read-only items.grudgebound.testnet talisman json-args '{"token_id":"tal-1"}' network-config testnet now
```

## TODOs (from the code)

- `rite`: validate rule templates and parameters on-chain (mirror `shared/src/rules.ts`).
- `rite`: apply money rules (R1, R4, R5) to battle results here instead of trusting the server's numbers.
- `rite`: let vows be settled as kept by anyone after they end; require evidence for breaches.
- `items`: finish NEP-245 (approvals, `mt_batch_transfer`, `mt_transfer_call` + resolve, metadata views).
- Both: integration tests on testnet (near-workspaces' sandbox has no Windows build).
