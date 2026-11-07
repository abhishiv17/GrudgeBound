import { KeyPair, type KeyPairString } from "near-api-js";
import { AuthError, type AuthStore, type StoredPlayer } from "./auth";
import { isAccountId, type ChainConfig, type ChainWriter } from "./chain";

/** Server-held signing is an explicit testnet-only option; it is never silently enabled. */
export function playerKeyResolver(store: AuthStore) {
  return (account: string): KeyPairString | undefined => store.data.players.find((p) => p.ready && p.account === account)?.signingKey as KeyPairString | undefined;
}

export function playerProvisioner(chain: ChainWriter | null, cfg: ChainConfig | null, keyMode: string | undefined) {
  return async (p: StoredPlayer, save: () => void) => {
    if (!chain || !cfg) {
      if (!p.account) p.account = `p${p.id}.offline`;
      p.ready = true; save(); return;
    }
    if (cfg.network !== "testnet" || keyMode !== "server") throw new AuthError(503, "The player signing method must be configured before testnet sign-in.");
    // An off-chain identity keeps its player id when the testnet integration is enabled later.
    if (p.ready && p.signingKey && p.account.endsWith(`.${cfg.gameAccount}`)) return;
    if (!p.signingKey) {
      p.signingKey = KeyPair.fromRandom("ed25519").toString();
      p.account = `p${p.id}.${cfg.gameAccount}`;
      p.ready = false;
      if (!isAccountId(p.account)) throw new Error("Game account is too long for player sub-accounts");
      save(); // preserve the recovery key before any transaction leaves this process
    }
    const key = KeyPair.fromString(p.signingKey as KeyPairString);
    await chain.provisionAccount(p.account, key.getPublicKey().toString());
    p.ready = true; save();
  };
}
