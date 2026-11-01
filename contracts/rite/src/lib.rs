//! The Rite: points, rules and vows, enforced on-chain (plan section 8.3).
//!
//! Status: Phase 1. Compiles to wasm; unit tests written; marked TODOs remain.
//! Build and test steps: contracts/README.md.
//!
//! Trust model: the game server reports battle results (with seed + log hash for replay);
//! the Arbiter account accepts or rejects proposed rules. Everything else (costs, refunds,
//! repeals, vow stakes) is enforced here, so nobody can quietly change a rule in force.

use near_sdk::serde_json::json;
use near_sdk::store::{LookupMap, Vector};
use near_sdk::{env, near, require, AccountId, PanicOnDefault};

pub const RULE_BASE_COST: u64 = 50;
pub const RULE_COST_STEP: u64 = 25;
pub const MAX_ACTIVE_RULES: u32 = 10;
pub const REPEAL_MULTIPLIER: u64 = 2;
pub const VOW_KEEP_BONUS_PERCENT: u64 = 20;

#[near(serializers = [borsh, json])]
#[derive(Clone, Debug, PartialEq)]
pub enum RuleStatus {
    Pending,
    Active,
    Rejected,
    Repealed,
}

#[near(serializers = [borsh, json])]
#[derive(Clone, Debug)]
pub struct Rule {
    pub id: u32,
    /// Template id from shared/src/rules.ts, e.g. "R8".
    pub template_id: String,
    /// JSON object with the template's parameters, e.g. {"ward":"lantern"}.
    pub params: String,
    pub author: AccountId,
    pub cost: u64,
    pub status: RuleStatus,
    pub reason: Option<String>,
    pub created_ms: u64,
}

#[near(serializers = [borsh, json])]
#[derive(Clone, Debug, PartialEq)]
pub enum VowStatus {
    Active,
    Kept,
    Broken,
}

#[near(serializers = [borsh, json])]
#[derive(Clone, Debug)]
pub struct Vow {
    pub id: u32,
    pub owner: AccountId,
    /// Template id from shared/src/vows.ts, e.g. "V1".
    pub template_id: String,
    pub params: String,
    pub stake: u64,
    pub status: VowStatus,
    pub created_ms: u64,
    pub ends_ms: u64,
}

#[near(serializers = [json])]
#[derive(Clone, Debug)]
pub struct PointsChange {
    pub account_id: AccountId,
    pub delta: i64,
}

#[near(serializers = [json])]
#[derive(Clone, Debug)]
pub struct BattleResult {
    pub battle_id: String,
    pub seed: u64,
    /// Hash of the full battle log, so anyone can replay and check it.
    pub log_hash: String,
    pub changes: Vec<PointsChange>,
}

#[near(contract_state)]
#[derive(PanicOnDefault)]
pub struct Rite {
    game_server: AccountId,
    arbiter: AccountId,
    points: LookupMap<AccountId, u64>,
    rules: Vector<Rule>,
    active_rule_count: u32,
    vows: Vector<Vow>,
    recorded_battles: LookupMap<String, bool>,
}

#[near]
impl Rite {
    #[init]
    pub fn new(game_server: AccountId, arbiter: AccountId) -> Self {
        Self {
            game_server,
            arbiter,
            points: LookupMap::new(b"p"),
            rules: Vector::new(b"r"),
            active_rule_count: 0,
            vows: Vector::new(b"v"),
            recorded_battles: LookupMap::new(b"b"),
        }
    }

    // ------------------------------------------------------------------ views

    /// Cost to add a rule now: 50 + 25 per rule already in force.
    pub fn rule_cost(&self) -> u64 {
        RULE_BASE_COST + RULE_COST_STEP * self.active_rule_count as u64
    }

    pub fn get_points(&self, account_id: AccountId) -> u64 {
        self.points.get(&account_id).copied().unwrap_or(0)
    }

    pub fn get_rules(&self, from_index: Option<u32>, limit: Option<u32>) -> Vec<Rule> {
        self.rules
            .iter()
            .skip(from_index.unwrap_or(0) as usize)
            .take(limit.unwrap_or(50) as usize)
            .cloned()
            .collect()
    }

    pub fn get_active_rules(&self) -> Vec<Rule> {
        self.rules.iter().filter(|r| r.status == RuleStatus::Active).cloned().collect()
    }

    pub fn get_vows(&self, account_id: AccountId) -> Vec<Vow> {
        self.vows.iter().filter(|v| v.owner == account_id).cloned().collect()
    }

    // ------------------------------------------------------------------ battles

    /// Game server only. Applies the points changes of one battle, once.
    /// TODO(seat 1): apply money rules here (R1 multipliers, R4 flee cost, R5 leader bonus)
    /// instead of trusting the server's numbers.
    pub fn submit_battle(&mut self, result: BattleResult) {
        self.assert_game_server();
        require!(self.recorded_battles.get(&result.battle_id).is_none(), "battle already recorded");
        self.recorded_battles.insert(result.battle_id.clone(), true);
        for c in result.changes.iter() {
            self.apply_points(&c.account_id, c.delta);
        }
        emit("battle", json!({ "battle_id": result.battle_id, "seed": result.seed, "log_hash": result.log_hash }));
    }

    // ------------------------------------------------------------------ rules

    /// Anyone with enough points. The cost is paid now and refunded if the Arbiter rejects.
    /// TODO(seat 1): validate template_id and params on-chain (mirror shared/src/rules.ts).
    pub fn propose_rule(&mut self, template_id: String, params: String) -> u32 {
        require!(self.active_rule_count < MAX_ACTIVE_RULES, "the Rite already holds the maximum number of rules");
        let author = env::predecessor_account_id();
        let cost = self.rule_cost();
        self.take_points(&author, cost);
        let id = self.rules.len();
        self.rules.push(Rule {
            id,
            template_id: template_id.clone(),
            params: params.clone(),
            author: author.clone(),
            cost,
            status: RuleStatus::Pending,
            reason: None,
            created_ms: env::block_timestamp_ms(),
        });
        emit("rule_proposed", json!({ "id": id, "template_id": template_id, "params": params, "author": author, "cost": cost }));
        id
    }

    /// Arbiter only. Accept (rule goes into force) or reject (full refund).
    pub fn resolve_rule(&mut self, rule_id: u32, accepted: bool, reason: Option<String>) {
        require!(env::predecessor_account_id() == self.arbiter, "only the Arbiter can resolve rules");
        let (author, cost) = {
            let rule = self.rules.get_mut(rule_id).unwrap_or_else(|| env::panic_str("no such rule"));
            require!(rule.status == RuleStatus::Pending, "rule is not pending");
            rule.status = if accepted { RuleStatus::Active } else { RuleStatus::Rejected };
            rule.reason = reason.clone();
            (rule.author.clone(), rule.cost)
        };
        if accepted {
            require!(self.active_rule_count < MAX_ACTIVE_RULES, "the Rite already holds the maximum number of rules");
            self.active_rule_count += 1;
        } else {
            self.add_points(&author, cost);
        }
        emit("rule_resolved", json!({ "id": rule_id, "accepted": accepted, "reason": reason }));
    }

    /// Anyone can repeal an active rule for twice its original cost.
    pub fn repeal_rule(&mut self, rule_id: u32) {
        let caller = env::predecessor_account_id();
        let cost = {
            let rule = self.rules.get(rule_id).unwrap_or_else(|| env::panic_str("no such rule"));
            require!(rule.status == RuleStatus::Active, "rule is not in force");
            rule.cost * REPEAL_MULTIPLIER
        };
        self.take_points(&caller, cost);
        if let Some(rule) = self.rules.get_mut(rule_id) {
            rule.status = RuleStatus::Repealed;
        }
        self.active_rule_count -= 1;
        emit("rule_repealed", json!({ "id": rule_id, "by": caller, "cost": cost }));
    }

    // ------------------------------------------------------------------ vows

    /// Lock a stake of points behind a vow.
    pub fn make_vow(&mut self, template_id: String, params: String, stake: u64, duration_ms: u64) -> u32 {
        require!(stake > 0, "stake must be positive");
        let owner = env::predecessor_account_id();
        self.take_points(&owner, stake);
        let id = self.vows.len();
        let now = env::block_timestamp_ms();
        self.vows.push(Vow {
            id,
            owner: owner.clone(),
            template_id: template_id.clone(),
            params,
            stake,
            status: VowStatus::Active,
            created_ms: now,
            ends_ms: now + duration_ms,
        });
        emit("vow_made", json!({ "id": id, "owner": owner, "template_id": template_id, "stake": stake }));
        id
    }

    /// Game server only. Kept: stake back +20%. Broken: stake burned.
    /// TODO(seat 1): let anyone settle a vow as kept after ends_ms; require evidence for breaches.
    pub fn settle_vow(&mut self, vow_id: u32, kept: bool) {
        self.assert_game_server();
        let (owner, stake) = {
            let vow = self.vows.get_mut(vow_id).unwrap_or_else(|| env::panic_str("no such vow"));
            require!(vow.status == VowStatus::Active, "vow already settled");
            vow.status = if kept { VowStatus::Kept } else { VowStatus::Broken };
            (vow.owner.clone(), vow.stake)
        };
        if kept {
            self.add_points(&owner, stake + stake * VOW_KEEP_BONUS_PERCENT / 100);
        }
        emit("vow_settled", json!({ "id": vow_id, "kept": kept }));
    }

    // ------------------------------------------------------------------ internal

    fn assert_game_server(&self) {
        require!(env::predecessor_account_id() == self.game_server, "only the game server");
    }

    fn add_points(&mut self, account: &AccountId, amount: u64) {
        let have = self.get_points(account.clone());
        self.points.insert(account.clone(), have + amount);
    }

    fn take_points(&mut self, account: &AccountId, amount: u64) {
        let have = self.get_points(account.clone());
        require!(have >= amount, "not enough points");
        self.points.insert(account.clone(), have - amount);
    }

    fn apply_points(&mut self, account: &AccountId, delta: i64) {
        if delta >= 0 {
            self.add_points(account, delta as u64);
        } else {
            let have = self.get_points(account.clone());
            self.points.insert(account.clone(), have.saturating_sub(delta.unsigned_abs()));
        }
    }
}

/// NEP-297 event log, read by the indexer.
fn emit(event: &str, data: near_sdk::serde_json::Value) {
    let payload = json!({ "standard": "grudgebound-rite", "version": "0.1.0", "event": event, "data": [data] });
    env::log_str(&format!("EVENT_JSON:{}", payload));
}

#[cfg(test)]
mod tests {
    use super::*;
    use near_sdk::test_utils::{accounts, VMContextBuilder};
    use near_sdk::testing_env;

    fn as_caller(who: AccountId) {
        testing_env!(VMContextBuilder::new().predecessor_account_id(who).build());
    }

    /// accounts(0) = game server, accounts(1) = arbiter, accounts(2) = alice, accounts(3) = bob.
    fn setup() -> Rite {
        as_caller(accounts(0));
        Rite::new(accounts(0), accounts(1))
    }

    fn give(c: &mut Rite, who: AccountId, points: i64, battle: &str) {
        as_caller(accounts(0));
        c.submit_battle(BattleResult {
            battle_id: battle.into(),
            seed: 7,
            log_hash: "hash".into(),
            changes: vec![PointsChange { account_id: who, delta: points }],
        });
    }

    #[test]
    fn battles_move_points_once() {
        let mut c = setup();
        give(&mut c, accounts(2), 80, "b1");
        assert_eq!(c.get_points(accounts(2)), 80);
        give(&mut c, accounts(2), -100, "b2");
        assert_eq!(c.get_points(accounts(2)), 0, "points never go below zero");
    }

    #[test]
    #[should_panic(expected = "battle already recorded")]
    fn a_battle_cannot_be_counted_twice() {
        let mut c = setup();
        give(&mut c, accounts(2), 5, "b1");
        give(&mut c, accounts(2), 5, "b1");
    }

    #[test]
    #[should_panic(expected = "only the game server")]
    fn players_cannot_report_battles() {
        let mut c = setup();
        as_caller(accounts(2));
        c.submit_battle(BattleResult { battle_id: "x".into(), seed: 1, log_hash: "h".into(), changes: vec![] });
    }

    #[test]
    fn propose_pays_now_and_rejection_refunds() {
        let mut c = setup();
        give(&mut c, accounts(2), 120, "b1");
        as_caller(accounts(2));
        let id = c.propose_rule("R8".into(), "{\"ward\":\"lantern\"}".into());
        assert_eq!(c.get_points(accounts(2)), 70, "cost 50 is taken at once");
        as_caller(accounts(1));
        c.resolve_rule(id, false, Some("contradicts R3".into()));
        assert_eq!(c.get_points(accounts(2)), 120, "a rejected rule is refunded in full");
        assert!(c.get_active_rules().is_empty());
        assert_eq!(c.get_rules(None, None)[0].status, RuleStatus::Rejected);
    }

    #[test]
    fn accepted_rules_raise_the_next_cost_and_repeal_costs_double() {
        let mut c = setup();
        give(&mut c, accounts(2), 200, "b1");
        give(&mut c, accounts(3), 200, "b2");
        assert_eq!(c.rule_cost(), 50);
        as_caller(accounts(2));
        let id = c.propose_rule("R1".into(), "{}".into());
        as_caller(accounts(1));
        c.resolve_rule(id, true, None);
        assert_eq!(c.get_active_rules().len(), 1);
        assert_eq!(c.rule_cost(), 75, "50 + 25 per rule in force");
        as_caller(accounts(3));
        c.repeal_rule(id);
        assert_eq!(c.get_points(accounts(3)), 100, "repeal costs 2 x 50");
        assert!(c.get_active_rules().is_empty());
        assert_eq!(c.rule_cost(), 50);
    }

    #[test]
    #[should_panic(expected = "not enough points")]
    fn cannot_propose_without_points() {
        let mut c = setup();
        as_caller(accounts(2));
        c.propose_rule("R1".into(), "{}".into());
    }

    #[test]
    #[should_panic(expected = "only the Arbiter can resolve rules")]
    fn only_the_arbiter_resolves() {
        let mut c = setup();
        give(&mut c, accounts(2), 60, "b1");
        as_caller(accounts(2));
        let id = c.propose_rule("R1".into(), "{}".into());
        c.resolve_rule(id, true, None);
    }

    #[test]
    #[should_panic(expected = "the Rite already holds the maximum number of rules")]
    fn at_most_ten_rules_in_force() {
        let mut c = setup();
        give(&mut c, accounts(2), 10_000, "b1");
        for _ in 0..=MAX_ACTIVE_RULES {
            as_caller(accounts(2));
            let id = c.propose_rule("R1".into(), "{}".into());
            as_caller(accounts(1));
            c.resolve_rule(id, true, None);
        }
    }

    #[test]
    fn kept_vows_pay_back_twenty_percent_and_broken_vows_burn() {
        let mut c = setup();
        give(&mut c, accounts(2), 100, "b1");
        as_caller(accounts(2));
        let kept = c.make_vow("V1".into(), "{}".into(), 50, 86_400_000);
        let broken = c.make_vow("V2".into(), "{}".into(), 50, 86_400_000);
        assert_eq!(c.get_points(accounts(2)), 0, "both stakes are locked");
        as_caller(accounts(0));
        c.settle_vow(kept, true);
        c.settle_vow(broken, false);
        assert_eq!(c.get_points(accounts(2)), 60, "50 back + 20%, the other stake burned");
        let vows = c.get_vows(accounts(2));
        assert_eq!(vows[0].status, VowStatus::Kept);
        assert_eq!(vows[1].status, VowStatus::Broken);
    }

    #[test]
    #[should_panic(expected = "vow already settled")]
    fn a_vow_settles_once() {
        let mut c = setup();
        give(&mut c, accounts(2), 50, "b1");
        as_caller(accounts(2));
        let id = c.make_vow("V1".into(), "{}".into(), 50, 1000);
        as_caller(accounts(0));
        c.settle_vow(id, true);
        c.settle_vow(id, true);
    }
}
