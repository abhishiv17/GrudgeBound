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

## Deploy (Phase 2)

Get the exact, current commands from **https://docs.near.org (Smart Contracts > Quickstart)**. Roughly:

1. Install `cargo-near` (build tool) and NEAR CLI. Build with `cargo near build`: it applies the wasm settings
   the NEAR runtime accepts. Check which Rust version the NEAR docs recommend; very new compilers can emit
   wasm features the runtime rejects.
2. Create the testnet accounts (`grudgebound.testnet` plus `items.` and `rite.` sub-accounts).
3. Deploy each contract and call `new(...)`.

Never commit keys. NEAR CLI stores credentials outside the repo; `.gitignore` also blocks common key files.

## TODOs (from the code)

- `rite`: validate rule templates and parameters on-chain (mirror `shared/src/rules.ts`).
- `rite`: apply money rules (R1, R4, R5) to battle results here instead of trusting the server's numbers.
- `rite`: let vows be settled as kept by anyone after they end; require evidence for breaches.
- `items`: finish NEP-245 (approvals, `mt_batch_transfer`, `mt_transfer_call` + resolve, metadata views).
- Both: a testnet deploy script; integration tests on testnet (near-workspaces' sandbox has no Windows build).
