//! Shared Storage (location `shared`, migration 0010): one per character, 5 tabs, opened at any storage shed. Outside
//! the PUT state: the game server saves the bag and the Storage together in one transaction, with the Storage version
//! it read (the market delivers items to Storage at any time). Each item's unit weight and the capacity come from the
//! game server; nw-core only sums them to reject a buy order over the capacity (`used_weight`).

use axum::extract::{Path, State};
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::GameServer;
use serde_json::Value;

use crate::error::{ApiError, ApiResult};
use crate::routes::characters::{empty_props, ItemInstance};
use crate::routes::internal::validate_inventory;
use crate::routes::journal::{self, Ctx, WriteSeq};
use crate::routes::serials;
use crate::AppState;

pub(crate) const MAX_STORAGE_SLOTS: i32 = 2000;
pub(crate) const SHARED: &str = "shared";
pub(crate) const TABS: i16 = 5;
const MAX_TAB_NAME: usize = 24;

/// Character's bag, equipped and coins after the action, applied in the same transaction as the action.
#[derive(Deserialize, Debug)]
pub struct Snapshot {
    pub inventory: Vec<ItemInstance>,
    pub coins: i64,
    /// Session, number and reason of the save (journal.rs).
    #[serde(flatten)]
    pub write: WriteSeq,
}

impl Snapshot {
    pub(crate) fn validate(&self) -> ApiResult<()> {
        if self.coins < 0 {
            return Err(ApiError::bad_request("invalid_coins", "Moedas negativas."));
        }
        validate_inventory(&self.inventory)
    }

    /// Serial numbers of the bag items (for `serials::verify` at the end of the transaction).
    pub(crate) fn serials(&self) -> Vec<String> {
        serials::serials(self.inventory.iter().map(|i| &i.props))
    }

    /// Checks the ordering (`journal::claim`) and saves bag and coins with the difference in the log.
    pub(crate) async fn apply(&self, tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<Ctx> {
        let ctx = journal::claim(tx, character_id, &self.write).await?;
        journal::write_bag(tx, &ctx, Some(&self.inventory), Some(self.coins)).await?;
        Ok(ctx)
    }
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, sqlx::FromRow)]
pub struct StorageItem {
    pub item_id: String,
    pub quantity: i32,
    pub slot: i32,
    #[serde(default)]
    pub gear_score: i32,
    #[serde(default)]
    pub rarity: i16,
    #[serde(default = "empty_props")]
    pub props: Value,
    /// Tab (0–4).
    #[serde(default)]
    pub tab: i16,
    /// Unit weight in kg (from the game server's catalog).
    #[serde(default)]
    pub weight: f32,
}

#[derive(Serialize, Debug)]
pub struct StorageView {
    pub version: i64,
    pub items: Vec<StorageItem>,
    /// Tab names set by the player (empty = default name).
    pub tab_names: Vec<String>,
    pub capacity: f32,
}

#[derive(Deserialize, Debug)]
pub struct StorageWrite {
    /// Version read in the GET; different from the current one = 409 `storage_changed` (read again and redo).
    pub version: i64,
    pub items: Vec<StorageItem>,
    /// Tab names (missing = keeps).
    pub tab_names: Option<Vec<String>>,
    /// Current capacity in kg, summed by the game server (missing = keeps).
    pub capacity: Option<f32>,
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

pub(crate) fn validate_location(location: &str) -> ApiResult<()> {
    if location == SHARED { Ok(()) } else { Err(ApiError::bad_request("invalid_location", format!("local inválido: {location}"))) }
}

/// Used Storage weight: stored items plus what the open buy orders will still deliver.
pub(crate) async fn used_weight(tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<f64> {
    let (used,): (f64,) = sqlx::query_as(
        "select coalesce((select sum(quantity * weight)::float8 from storage_items where character_id = $1 and location = 'shared'), 0)
              + coalesce((select sum(quantity * weight)::float8 from market_orders
                           where character_id = $1 and kind = 'buy' and status = 'open'), 0)",
    )
    .bind(character_id)
    .fetch_one(&mut *tx)
    .await?;
    Ok(used)
}

/// Capacity stored by the game server.
pub(crate) async fn capacity(tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<f64> {
    let (cap,): (f32,) = sqlx::query_as("select storage_capacity from characters where id = $1")
        .bind(character_id)
        .fetch_optional(&mut *tx)
        .await?
        .ok_or_else(|| ApiError::not_found("Personagem não encontrado."))?;
    Ok(cap as f64)
}

/// Rejects (409 `storage_full`) a purchase of `extra` kg that would exceed the Storage capacity.
pub(crate) async fn check_room(tx: &mut sqlx::PgConnection, character_id: Uuid, extra: f64) -> ApiResult<()> {
    let used = used_weight(tx, character_id).await?;
    let cap = capacity(tx, character_id).await?;
    if used + extra > cap + 1e-3 {
        return Err(ApiError::conflict("storage_full", format!("Armazém sem espaço: {used:.1} + {extra:.1} kg de {cap:.1} kg.")));
    }
    Ok(())
}

pub(crate) fn is_territory(t: &str) -> bool {
    !t.is_empty() && t.len() <= 40 && t.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'_')
}

/// Ensures the Storage row exists and locks it; returns the current version.
pub(crate) async fn lock_storage(tx: &mut sqlx::PgConnection, character_id: Uuid, location: &str) -> ApiResult<i64> {
    sqlx::query("insert into storages (character_id, location) values ($1, $2) on conflict do nothing")
        .bind(character_id)
        .bind(location)
        .execute(&mut *tx)
        .await?;
    let (version,): (i64,) = sqlx::query_as("select version from storages where character_id = $1 and location = $2 for update")
        .bind(character_id)
        .bind(location)
        .fetch_one(&mut *tx)
        .await?;
    Ok(version)
}

async fn bump(tx: &mut sqlx::PgConnection, character_id: Uuid, location: &str) -> ApiResult<i64> {
    let (version,): (i64,) =
        sqlx::query_as("update storages set version = version + 1 where character_id = $1 and location = $2 returning version")
            .bind(character_id)
            .bind(location)
            .fetch_one(&mut *tx)
            .await?;
    Ok(version)
}

/// Delivers an item to Storage (market), even over capacity: adds to the matching stack (same `props`, any tab)
/// or takes the next slot on the first tab (`tab` 0; tabs are 0-based, the screen shows them as 1–5).
#[allow(clippy::too_many_arguments)]
pub(crate) async fn deliver(
    tx: &mut sqlx::PgConnection,
    character_id: Uuid,
    location: &str,
    item_id: &str,
    quantity: i32,
    gear_score: i32,
    rarity: i16,
    props: &Value,
    weight: f32,
    reason: &str,
) -> ApiResult<()> {
    lock_storage(tx, character_id, location).await?;
    let merged: Option<(i16,)> = sqlx::query_as(
        "update storage_items set quantity = quantity + $6
          where id = (select id from storage_items
                       where character_id = $1 and location = $2 and item_id = $3 and gear_score = $4 and rarity = $5
                         and props = $7
                       order by slot limit 1)
          returning tab",
    )
    .bind(character_id)
    .bind(location)
    .bind(item_id)
    .bind(gear_score)
    .bind(rarity)
    .bind(quantity)
    .bind(props)
    .fetch_optional(&mut *tx)
    .await?;
    let tab = merged.map_or(0, |m| m.0);
    journal::log(tx, &Ctx::core(character_id, reason), &format!("storage:{tab}"), Some(item_id), quantity as i64, gear_score, rarity, props).await?;
    if merged.is_none() {
        let (slot,): (i32,) = sqlx::query_as(
            "select coalesce(max(slot) + 1, 0) from storage_items where character_id = $1 and location = $2",
        )
        .bind(character_id)
        .bind(location)
        .fetch_one(&mut *tx)
        .await?;
        sqlx::query(
            "insert into storage_items (id, character_id, location, item_id, quantity, slot, gear_score, rarity, props, tab, weight)
             values ($1, $2, $3, $4, $5, $6, $7, $8, $9, 0, $10)",
        )
        .bind(Uuid::new_v4())
        .bind(character_id)
        .bind(location)
        .bind(item_id)
        .bind(quantity)
        .bind(slot)
        .bind(gear_score)
        .bind(rarity)
        .bind(props)
        .bind(weight)
        .execute(&mut *tx)
        .await?;
    }
    bump(tx, character_id, location).await?;
    Ok(())
}

pub async fn get(
    State(state): State<AppState>,
    _server: GameServer,
    Path((id, location)): Path<(Uuid, String)>,
) -> ApiResult<Json<StorageView>> {
    validate_location(&location)?;
    let head: Option<(i64, Value)> = sqlx::query_as("select version, tab_names from storages where character_id = $1 and location = $2")
        .bind(id)
        .bind(&location)
        .fetch_optional(&state.db)
        .await?;
    let items: Vec<StorageItem> = sqlx::query_as(
        "select item_id, quantity, slot, gear_score, rarity, props, tab, weight from storage_items
          where character_id = $1 and location = $2 order by slot",
    )
    .bind(id)
    .bind(&location)
    .fetch_all(&state.db)
    .await?;
    let (cap,): (f32,) = sqlx::query_as("select storage_capacity from characters where id = $1")
        .bind(id)
        .fetch_optional(&state.db)
        .await?
        .ok_or_else(|| ApiError::not_found("Personagem não encontrado."))?;
    let (version, names) = head.unwrap_or((0, Value::Array(vec![])));
    Ok(Json(StorageView { version, items, tab_names: tab_names(&names), capacity: cap }))
}

fn tab_names(v: &Value) -> Vec<String> {
    v.as_array().map(|a| a.iter().map(|n| n.as_str().unwrap_or_default().to_string()).collect()).unwrap_or_default()
}

pub async fn put(
    State(state): State<AppState>,
    _server: GameServer,
    Path((id, location)): Path<(Uuid, String)>,
    Json(w): Json<StorageWrite>,
) -> ApiResult<Json<StorageView>> {
    validate_location(&location)?;
    w.snapshot.validate()?;
    let mut seen = std::collections::HashSet::new();
    for it in &w.items {
        let item = ItemInstance {
            item_id: it.item_id.clone(),
            quantity: it.quantity,
            container: "bag".into(),
            slot: 0,
            gear_score: it.gear_score,
            rarity: it.rarity,
            props: it.props.clone(),
        };
        item.validate().map_err(|e| ApiError::bad_request("invalid_storage", e))?;
        if !(0..MAX_STORAGE_SLOTS).contains(&it.slot) || !seen.insert(it.slot) {
            return Err(ApiError::bad_request("invalid_storage", format!("slot inválido ou repetido: {}", it.slot)));
        }
        if !(0..TABS).contains(&it.tab) || !it.weight.is_finite() || it.weight < 0.0 {
            return Err(ApiError::bad_request("invalid_storage", format!("aba ou peso inválido no slot {}", it.slot)));
        }
    }
    if let Some(names) = &w.tab_names {
        if names.len() > TABS as usize || names.iter().any(|n| n.chars().count() > MAX_TAB_NAME) {
            return Err(ApiError::bad_request("invalid_storage", "Nomes de aba inválidos."));
        }
    }
    if w.capacity.is_some_and(|c| !c.is_finite() || c < 0.0) {
        return Err(ApiError::bad_request("invalid_storage", "Capacidade inválida."));
    }
    let mut tx = state.db.begin().await?;
    let ctx = w.snapshot.apply(&mut tx, id).await?;
    if let Some(cap) = w.capacity {
        sqlx::query("update characters set storage_capacity = $2 where id = $1")
            .bind(id)
            .bind(cap)
            .execute(&mut *tx)
            .await?;
    }
    let current = lock_storage(&mut tx, id, &location).await?;
    if current != w.version {
        return Err(ApiError::conflict("storage_changed", "O armazém mudou; leia de novo."));
    }
    let before = journal::storage_tally(&mut tx, id).await?;
    sqlx::query("delete from storage_items where character_id = $1 and location = $2")
        .bind(id)
        .bind(&location)
        .execute(&mut *tx)
        .await?;
    if let Some(names) = &w.tab_names {
        sqlx::query("update storages set tab_names = $3 where character_id = $1 and location = $2")
            .bind(id)
            .bind(&location)
            .bind(serde_json::json!(names))
            .execute(&mut *tx)
            .await?;
    }
    for it in &w.items {
        sqlx::query(
            "insert into storage_items (id, character_id, location, item_id, quantity, slot, gear_score, rarity, props, tab, weight)
             values ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)",
        )
        .bind(Uuid::new_v4())
        .bind(id)
        .bind(&location)
        .bind(&it.item_id)
        .bind(it.quantity)
        .bind(it.slot)
        .bind(it.gear_score)
        .bind(it.rarity)
        .bind(&it.props)
        .bind(it.tab)
        .bind(it.weight)
        .execute(&mut *tx)
        .await?;
    }
    let after = journal::storage_tally(&mut tx, id).await?;
    journal::log_diff(&mut tx, &ctx, &before, &after).await?;
    let version = bump(&mut tx, id, &location).await?;
    serials::verify(&mut tx, id, "bag", &w.snapshot.serials()).await?;
    serials::verify(&mut tx, id, &location, &serials::serials(w.items.iter().map(|i| &i.props))).await?;
    let (names, cap): (Value, f32) = sqlx::query_as(
        "select s.tab_names, c.storage_capacity from storages s join characters c on c.id = s.character_id
          where s.character_id = $1 and s.location = $2",
    )
    .bind(id)
    .bind(&location)
    .fetch_one(&mut *tx)
    .await?;
    tx.commit().await?;
    Ok(Json(StorageView { version, items: w.items, tab_names: tab_names(&names), capacity: cap }))
}
