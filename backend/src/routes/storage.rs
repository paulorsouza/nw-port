//! Armazém da cidade e da casa. Fora do PUT state: o servidor de jogo grava a bolsa e o armazém juntos numa
//! transação, com a versão do armazém que ele leu (o mercado entrega itens no armazém a qualquer momento).

use axum::extract::{Path, State};
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::GameServer;
use serde_json::Value;

use crate::error::{ApiError, ApiResult};
use crate::routes::characters::{empty_props, ItemInstance};
use crate::routes::internal::{validate_inventory, write_inventory};
use crate::routes::serials;
use crate::AppState;

pub(crate) const MAX_STORAGE_SLOTS: i32 = 2000;

/// Bolsa, equipado e moedas do personagem depois da ação, aplicados na mesma transação da ação.
#[derive(Deserialize, Debug)]
pub struct Snapshot {
    pub inventory: Vec<ItemInstance>,
    pub coins: i64,
}

impl Snapshot {
    pub(crate) fn validate(&self) -> ApiResult<()> {
        if self.coins < 0 {
            return Err(ApiError::bad_request("invalid_coins", "Moedas negativas."));
        }
        validate_inventory(&self.inventory)
    }

    /// Números de série dos itens da bolsa (para `serials::verify` no fim da transação).
    pub(crate) fn serials(&self) -> Vec<String> {
        serials::serials(self.inventory.iter().map(|i| &i.props))
    }

    pub(crate) async fn apply(&self, tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<()> {
        let done = sqlx::query("update characters set coins = $2 where id = $1 and deleted_at is null")
            .bind(character_id)
            .bind(self.coins)
            .execute(&mut *tx)
            .await?;
        if done.rows_affected() == 0 {
            return Err(ApiError::not_found("Personagem"));
        }
        write_inventory(tx, character_id, &self.inventory).await
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
}

#[derive(Serialize, Debug)]
pub struct StorageView {
    pub version: i64,
    pub items: Vec<StorageItem>,
}

#[derive(Deserialize, Debug)]
pub struct StorageWrite {
    /// Versão lida no GET; diferente da atual = 409 `storage_changed` (ler de novo e refazer).
    pub version: i64,
    pub items: Vec<StorageItem>,
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

pub(crate) fn validate_location(location: &str) -> ApiResult<()> {
    let ok = match location.split_once(':') {
        Some(("settlement", t)) => is_territory(t),
        Some(("house", id)) => Uuid::parse_str(id).is_ok(),
        _ => false,
    };
    if ok { Ok(()) } else { Err(ApiError::bad_request("invalid_location", format!("local inválido: {location}"))) }
}

pub(crate) fn is_territory(t: &str) -> bool {
    !t.is_empty() && t.len() <= 40 && t.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'_')
}

/// Garante a linha do armazém e trava; devolve a versão atual.
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

/// Entrega um item no armazém (mercado): soma na pilha igual (mesmos `props`) ou ocupa o próximo slot.
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
) -> ApiResult<()> {
    lock_storage(tx, character_id, location).await?;
    let merged = sqlx::query(
        "update storage_items set quantity = quantity + $6
          where id = (select id from storage_items
                       where character_id = $1 and location = $2 and item_id = $3 and gear_score = $4 and rarity = $5
                         and props = $7
                       order by slot limit 1)",
    )
    .bind(character_id)
    .bind(location)
    .bind(item_id)
    .bind(gear_score)
    .bind(rarity)
    .bind(quantity)
    .bind(props)
    .execute(&mut *tx)
    .await?;
    if merged.rows_affected() == 0 {
        let (slot,): (i32,) = sqlx::query_as(
            "select coalesce(max(slot) + 1, 0) from storage_items where character_id = $1 and location = $2",
        )
        .bind(character_id)
        .bind(location)
        .fetch_one(&mut *tx)
        .await?;
        sqlx::query(
            "insert into storage_items (id, character_id, location, item_id, quantity, slot, gear_score, rarity, props)
             values ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
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
    let version: Option<(i64,)> = sqlx::query_as("select version from storages where character_id = $1 and location = $2")
        .bind(id)
        .bind(&location)
        .fetch_optional(&state.db)
        .await?;
    let items: Vec<StorageItem> = sqlx::query_as(
        "select item_id, quantity, slot, gear_score, rarity, props from storage_items
          where character_id = $1 and location = $2 order by slot",
    )
    .bind(id)
    .bind(&location)
    .fetch_all(&state.db)
    .await?;
    Ok(Json(StorageView { version: version.map_or(0, |v| v.0), items }))
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
    }
    let mut tx = state.db.begin().await?;
    if location.starts_with("house:") {
        let house = Uuid::parse_str(&location["house:".len()..]).unwrap_or_default();
        let owned: Option<(Uuid,)> = sqlx::query_as("select id from houses where id = $1 and character_id = $2")
            .bind(house)
            .bind(id)
            .fetch_optional(&mut *tx)
            .await?;
        owned.ok_or_else(|| ApiError::not_found("Casa"))?;
    }
    w.snapshot.apply(&mut tx, id).await?;
    let current = lock_storage(&mut tx, id, &location).await?;
    if current != w.version {
        return Err(ApiError::conflict("storage_changed", "O armazém mudou; leia de novo."));
    }
    sqlx::query("delete from storage_items where character_id = $1 and location = $2")
        .bind(id)
        .bind(&location)
        .execute(&mut *tx)
        .await?;
    for it in &w.items {
        sqlx::query(
            "insert into storage_items (id, character_id, location, item_id, quantity, slot, gear_score, rarity, props)
             values ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
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
        .execute(&mut *tx)
        .await?;
    }
    let version = bump(&mut tx, id, &location).await?;
    serials::verify(&mut tx, id, "bag", &w.snapshot.serials()).await?;
    serials::verify(&mut tx, id, &location, &serials::serials(w.items.iter().map(|i| &i.props))).await?;
    tx.commit().await?;
    Ok(Json(StorageView { version, items: w.items }))
}
