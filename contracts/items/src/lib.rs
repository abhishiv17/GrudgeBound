//! Grudgebound items: the Ember coin and every talisman in one multi-token contract.
//!
//! Status: Day 1 skeleton. It follows the shape of NEP-245 (multi token) but is NOT yet
//! fully compliant: approvals, mt_batch_transfer, mt_transfer_call and resolve are TODO.
//! Read the spec before finishing it: https://github.com/near/NEPs (neps/nep-0245.md).
//! Not yet compiled on the team machine: install Rust + cargo-near first (see contracts/README.md).

use near_sdk::json_types::U128;
use near_sdk::serde_json::json;
use near_sdk::store::LookupMap;
use near_sdk::{assert_one_yocto, env, near, require, AccountId, PanicOnDefault};

pub type TokenId = String;
pub const EMBER: &str = "ember";

#[near(serializers = [borsh, json])]
#[derive(Clone, Debug)]
pub struct TalismanMeta {
    /// Curse species id from shared/src/curses.ts, e.g. "hushmoth".
    pub species: String,
    pub emotion: String,
    pub omen: String,
    pub level: u8,
    pub wins: u32,
}

#[near(serializers = [json])]
pub struct TalismanView {
    pub token_id: TokenId,
    pub owner: AccountId,
    pub meta: TalismanMeta,
    /// Ember this talisman has earned. It moves with the token when traded.
    pub purse: U128,
}

#[near(contract_state)]
#[derive(PanicOnDefault)]
pub struct Items {
    /// The account allowed to mint (the Rite contract or the game server).
    minter: AccountId,
    /// Balance per "{token_id}:{account_id}".
    balances: LookupMap<String, u128>,
    talismans: LookupMap<TokenId, TalismanMeta>,
    owners: LookupMap<TokenId, AccountId>,
    purses: LookupMap<TokenId, u128>,
    next_talisman: u64,
    ember_supply: u128,
}

#[near]
impl Items {
    #[init]
    pub fn new(minter: AccountId) -> Self {
        Self {
            minter,
            balances: LookupMap::new(b"b"),
            talismans: LookupMap::new(b"t"),
            owners: LookupMap::new(b"o"),
            purses: LookupMap::new(b"u"),
            next_talisman: 1,
            ember_supply: 0,
        }
    }

    // ------------------------------------------------------------------ minting (minter only)

    pub fn mint_ember(&mut self, to: AccountId, amount: U128) {
        self.assert_minter();
        self.add(EMBER, &to, amount.0);
        self.ember_supply += amount.0;
        emit_mint(&to, EMBER, amount.0);
    }

    /// Called when a curse is bound. Returns the new talisman's token id.
    pub fn mint_talisman(&mut self, to: AccountId, meta: TalismanMeta) -> TokenId {
        self.assert_minter();
        let token_id = format!("tal-{}", self.next_talisman);
        self.next_talisman += 1;
        self.talismans.insert(token_id.clone(), meta);
        self.owners.insert(token_id.clone(), to.clone());
        self.add(&token_id, &to, 1);
        emit_mint(&to, &token_id, 1);
        token_id
    }

    /// Bounty reward paid into a talisman's purse.
    pub fn credit_purse(&mut self, token_id: TokenId, amount: U128) {
        self.assert_minter();
        require!(self.owners.get(&token_id).is_some(), "no such talisman");
        let have = self.purses.get(&token_id).copied().unwrap_or(0);
        self.purses.insert(token_id, have + amount.0);
        self.ember_supply += amount.0;
    }

    /// TODO(seat 1): record wins and level-ups reported by the game server.
    pub fn record_win(&mut self, token_id: TokenId) {
        self.assert_minter();
        let meta = self.talismans.get_mut(&token_id).unwrap_or_else(|| env::panic_str("no such talisman"));
        meta.wins += 1;
    }

    // ------------------------------------------------------------------ owner actions

    /// Move a talisman's purse into the owner's Ember balance.
    pub fn withdraw_purse(&mut self, token_id: TokenId) -> U128 {
        let owner = self.owners.get(&token_id).cloned().unwrap_or_else(|| env::panic_str("no such talisman"));
        require!(env::predecessor_account_id() == owner, "only the owner");
        let amount = self.purses.insert(token_id, 0).unwrap_or(0);
        self.add(EMBER, &owner, amount);
        U128(amount)
    }

    /// NEP-245 style transfer (simplified). Requires exactly 1 yoctoNEAR, as the standard does.
    #[payable]
    pub fn mt_transfer(&mut self, receiver_id: AccountId, token_id: TokenId, amount: U128, memo: Option<String>) {
        assert_one_yocto();
        let sender = env::predecessor_account_id();
        require!(sender != receiver_id, "cannot transfer to yourself");
        require!(amount.0 > 0, "amount must be positive");
        self.sub(&token_id, &sender, amount.0);
        self.add(&token_id, &receiver_id, amount.0);
        if self.owners.get(&token_id).is_some() {
            self.owners.insert(token_id.clone(), receiver_id.clone());
        }
        let data = json!({
            "old_owner_id": sender, "new_owner_id": receiver_id,
            "token_ids": [token_id], "amounts": [amount.0.to_string()], "memo": memo,
        });
        emit("mt_transfer", data);
    }

    // ------------------------------------------------------------------ views

    pub fn mt_balance_of(&self, account_id: AccountId, token_id: TokenId) -> U128 {
        U128(self.balance(&token_id, &account_id))
    }

    pub fn mt_batch_balance_of(&self, account_id: AccountId, token_ids: Vec<TokenId>) -> Vec<U128> {
        token_ids.iter().map(|t| U128(self.balance(t, &account_id))).collect()
    }

    pub fn talisman(&self, token_id: TokenId) -> Option<TalismanView> {
        let meta = self.talismans.get(&token_id)?.clone();
        let owner = self.owners.get(&token_id)?.clone();
        let purse = self.purses.get(&token_id).copied().unwrap_or(0);
        Some(TalismanView { token_id, owner, meta, purse: U128(purse) })
    }

    pub fn ember_supply(&self) -> U128 {
        U128(self.ember_supply)
    }

    // ------------------------------------------------------------------ internal

    fn assert_minter(&self) {
        require!(env::predecessor_account_id() == self.minter, "only the minter");
    }

    fn key(token_id: &str, account: &AccountId) -> String {
        format!("{}:{}", token_id, account)
    }

    fn balance(&self, token_id: &str, account: &AccountId) -> u128 {
        self.balances.get(&Self::key(token_id, account)).copied().unwrap_or(0)
    }

    fn add(&mut self, token_id: &str, account: &AccountId, amount: u128) {
        let b = self.balance(token_id, account);
        self.balances.insert(Self::key(token_id, account), b + amount);
    }

    fn sub(&mut self, token_id: &str, account: &AccountId, amount: u128) {
        let b = self.balance(token_id, account);
        require!(b >= amount, "insufficient balance");
        self.balances.insert(Self::key(token_id, account), b - amount);
    }
}

fn emit(event: &str, data: near_sdk::serde_json::Value) {
    let payload = json!({ "standard": "nep245", "version": "1.0.0", "event": event, "data": [data] });
    env::log_str(&format!("EVENT_JSON:{}", payload));
}

fn emit_mint(owner: &AccountId, token_id: &str, amount: u128) {
    emit("mt_mint", json!({ "owner_id": owner, "token_ids": [token_id], "amounts": [amount.to_string()] }));
}
