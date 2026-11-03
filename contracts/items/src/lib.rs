//! Grudgebound items: the Ember coin and every talisman in one multi-token contract.
//!
//! Status: Phase 1. Compiles to wasm; unit tests written. It follows the shape of NEP-245 (multi token) but is NOT yet
//! fully compliant: approvals, mt_batch_transfer, mt_transfer_call and resolve are TODO.
//! Read the spec before finishing it: https://github.com/near/NEPs (neps/nep-0245.md).
//! Build and test steps: contracts/README.md.

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

#[near(serializers = [json])]
pub struct TalismanPage {
    pub talismans: Vec<TalismanView>,
    /// Pass as `from_index` for the next page; null when every talisman has been looked at.
    pub next_index: Option<u64>,
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

    /// Talismans owned by `account_id`, oldest first. Walks token numbers from `from_index` (default 1), looking at
    /// most `limit` numbers (default and cap 200), so a client pages with the returned `next_index`.
    /// A linear scan is fine at game scale; an owner index would replace it if talismans number in the tens of thousands.
    pub fn talismans_of(&self, account_id: AccountId, from_index: Option<u64>, limit: Option<u64>) -> TalismanPage {
        let start = from_index.unwrap_or(1).max(1);
        let end = start.saturating_add(limit.unwrap_or(200).min(200)).min(self.next_talisman);
        let talismans = (start..end)
            .map(|n| format!("tal-{}", n))
            .filter(|id| self.owners.get(id) == Some(&account_id))
            .filter_map(|id| self.talisman(id))
            .collect();
        TalismanPage { talismans, next_index: if end < self.next_talisman { Some(end) } else { None } }
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

#[cfg(test)]
mod tests {
    use super::*;
    use near_sdk::test_utils::{accounts, VMContextBuilder};
    use near_sdk::{testing_env, NearToken};

    fn as_caller(who: AccountId, yocto: u128) {
        let ctx = VMContextBuilder::new()
            .predecessor_account_id(who)
            .attached_deposit(NearToken::from_yoctonear(yocto))
            .build();
        testing_env!(ctx);
    }

    fn meta(species: &str) -> TalismanMeta {
        TalismanMeta { species: species.into(), emotion: "sorrow".into(), omen: "whisper".into(), level: 1, wins: 0 }
    }

    /// accounts(0) = minter (game server), accounts(1) = alice, accounts(2) = bob.
    fn setup() -> Items {
        as_caller(accounts(0), 0);
        Items::new(accounts(0))
    }

    #[test]
    fn minter_mints_ember_and_talismans() {
        let mut c = setup();
        c.mint_ember(accounts(1), U128(100));
        let id = c.mint_talisman(accounts(1), meta("weepwick"));
        assert_eq!(id, "tal-1");
        assert_eq!(c.mint_talisman(accounts(2), meta("hushmoth")), "tal-2");
        assert_eq!(c.mt_balance_of(accounts(1), EMBER.into()).0, 100);
        assert_eq!(c.mt_balance_of(accounts(1), id.clone()).0, 1);
        assert_eq!(c.ember_supply().0, 100);
        let view = c.talisman(id).unwrap();
        assert_eq!(view.owner, accounts(1));
        assert_eq!(view.meta.species, "weepwick");
    }

    #[test]
    fn talismans_of_lists_what_an_account_owns_and_pages() {
        let mut c = setup();
        for i in 0..5 {
            c.mint_talisman(if i % 2 == 0 { accounts(1) } else { accounts(2) }, meta("weepwick"));
        }
        as_caller(accounts(1), 1);
        c.mt_transfer(accounts(2), "tal-1".into(), U128(1), None);
        let all = c.talismans_of(accounts(2), None, None);
        let ids: Vec<_> = all.talismans.iter().map(|t| t.token_id.clone()).collect();
        assert_eq!(ids, vec!["tal-1", "tal-2", "tal-4"]);
        assert_eq!(all.next_index, None);
        let first = c.talismans_of(accounts(1), Some(1), Some(3));
        assert_eq!(first.talismans.len(), 1, "tal-3 only: tal-1 was given away");
        assert_eq!(first.next_index, Some(4));
        let rest = c.talismans_of(accounts(1), first.next_index, Some(3));
        assert_eq!(rest.talismans[0].token_id, "tal-5");
        assert_eq!(rest.next_index, None);
    }

    #[test]
    #[should_panic(expected = "only the minter")]
    fn others_cannot_mint() {
        let mut c = setup();
        as_caller(accounts(1), 0);
        c.mint_talisman(accounts(1), meta("weepwick"));
    }

    #[test]
    fn transfer_moves_the_talisman_and_its_purse() {
        let mut c = setup();
        let id = c.mint_talisman(accounts(1), meta("weepwick"));
        c.credit_purse(id.clone(), U128(30));
        as_caller(accounts(1), 1);
        c.mt_transfer(accounts(2), id.clone(), U128(1), None);
        assert_eq!(c.mt_balance_of(accounts(1), id.clone()).0, 0);
        assert_eq!(c.mt_balance_of(accounts(2), id.clone()).0, 1);
        let view = c.talisman(id.clone()).unwrap();
        assert_eq!(view.owner, accounts(2));
        assert_eq!(view.purse.0, 30, "the purse travels with the talisman");
        // the new owner can empty the purse into their Ember
        as_caller(accounts(2), 0);
        assert_eq!(c.withdraw_purse(id.clone()).0, 30);
        assert_eq!(c.mt_balance_of(accounts(2), EMBER.into()).0, 30);
        assert_eq!(c.talisman(id).unwrap().purse.0, 0);
    }

    #[test]
    #[should_panic(expected = "only the owner")]
    fn only_the_owner_withdraws_a_purse() {
        let mut c = setup();
        let id = c.mint_talisman(accounts(1), meta("weepwick"));
        c.credit_purse(id.clone(), U128(30));
        as_caller(accounts(2), 0);
        c.withdraw_purse(id);
    }

    #[test]
    #[should_panic(expected = "insufficient balance")]
    fn cannot_transfer_what_you_do_not_hold() {
        let mut c = setup();
        let id = c.mint_talisman(accounts(1), meta("weepwick"));
        as_caller(accounts(2), 1);
        c.mt_transfer(accounts(3), id, U128(1), None);
    }

    #[test]
    #[should_panic(expected = "Requires attached deposit of exactly 1 yoctoNEAR")]
    fn transfer_needs_one_yocto() {
        let mut c = setup();
        let id = c.mint_talisman(accounts(1), meta("weepwick"));
        as_caller(accounts(1), 0);
        c.mt_transfer(accounts(2), id, U128(1), None);
    }

    #[test]
    fn wins_are_recorded() {
        let mut c = setup();
        let id = c.mint_talisman(accounts(1), meta("weepwick"));
        c.record_win(id.clone());
        c.record_win(id.clone());
        assert_eq!(c.talisman(id).unwrap().meta.wins, 2);
    }
}
