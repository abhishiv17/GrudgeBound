//! The Rite: points, rules and vows, enforced on-chain (plan section 8.3).
//!
//! Status: Day 1 skeleton. Core bookkeeping works; marked TODOs remain.
//! Not yet compiled on the team machine: install Rust + cargo-near first (see contracts/README.md).
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
