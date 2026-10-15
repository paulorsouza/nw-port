//! Bags on the ground (migration 0012; step 2 of the saves). Dropping and taking store the bag and the ground bag in the
//! same transaction, with session and number (journal.rs); the ground bag comes back in the same place if the game
//! server restarts before it expires.

use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use time::{Duration, OffsetDateTime};
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{ApiError, ApiResult};
use crate::routes::characters::{ItemInstance, Position};
use crate::routes::journal::{self, Ctx, Tally};
use crate::routes::serials;
use crate::routes::storage::{Snapshot, StorageItem};
use crate::AppState;

const MAX_TTL: i64 = 3600;
const MAX_ITEMS: usize = 500;

#[derive(Deserialize, Debug)]
pub struct DropRequest {
    pub map: String,
    pub position: Position,
    /// Seconds until it disappears (the game uses 180).
    pub ttl_seconds: i64,
    pub items: Vec<StorageItem>,
    /// Bag and coins after dropping, with session, number and reason.
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

#[derive(Deserialize, Debug)]
pub struct TakeRequest {
    pub character_id: Uuid,
    /// What stays in the ground bag after taking (empty = the ground bag disappears).
    pub items: Vec<StorageItem>,
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

#[derive(Serialize, Debug)]
pub struct Bag {
    pub id: Uuid,
    pub map: String,
    pub position: Position,
    #[serde(with = "time::serde::rfc3339")]
    pub expires_at: OffsetDateTime,
    pub items: Vec<StorageItem>,
}

fn validate_items(items: &[StorageItem], allow_empty: bool) -> ApiResult<()> {
    if (!allow_empty && items.is_empty()) || items.len() > MAX_ITEMS {
        return Err(ApiError::bad_request("invalid_ground", "Itens do saco inválidos."));
    }
    let mut seen = std::collections::HashSet::new();
    for it in items {
        let check = ItemInstance {
            item_id: it.item_id.clone(),
            quantity: it.quantity,
            container: "bag".into(),
            slot: 0,
            gear_score: it.gear_score,
            rarity: it.rarity,
            props: it.props.clone(),
        };
        check.validate().map_err(|e| ApiError::bad_request("invalid_ground", e))?;
        if it.slot < 0 || !seen.insert(it.slot) || !it.weight.is_finite() || it.weight < 0.0 {
            return Err(ApiError::bad_request("invalid_ground", format!("slot ou peso inválido: {}", it.slot)));
        }
    }
    Ok(())
}

fn place(bag: Uuid) -> String {
    format!("ground:{bag}")
}

async fn items_of(tx: &mut sqlx::PgConnection, bag: Uuid) -> ApiResult<Vec<StorageItem>> {
    Ok(sqlx::query_as(
        "select item_id, quantity, slot, gear_score, rarity, props, 0::smallint as tab, weight from ground_items
          where bag_id = $1 order by slot",
    )
    .bind(bag)
    .fetch_all(&mut *tx)
    .await?)
}

fn tally(bag: Uuid, items: &[StorageItem]) -> Tally {
    let mut t = Tally::new();
    for it in items {
        journal::add(&mut t, &place(bag), &it.item_id, it.quantity as i64, it.gear_score, it.rarity, &it.props);
    }
    t
}

async fn write_items(tx: &mut sqlx::PgConnection, bag: Uuid, items: &[StorageItem]) -> ApiResult<()> {
    sqlx::query("delete from ground_items where bag_id = $1").bind(bag).execute(&mut *tx).await?;
    for it in items {
        sqlx::query(
            "insert into ground_items (bag_id, slot, item_id, quantity, gear_score, rarity, props, weight)
             values ($1, $2, $3, $4, $5, $6, $7, $8)",
        )
        .bind(bag)
        .bind(it.slot)
        .bind(&it.item_id)
        .bind(it.quantity)
        .bind(it.gear_score)
        .bind(it.rarity)
        .bind(&it.props)
        .bind(it.weight)
        .execute(&mut *tx)
        .await?;
    }
    Ok(())
}

/// Deletes expired ground bags (of one character or of all), logging the removal.
pub(crate) async fn expire(tx: &mut sqlx::PgConnection, character_id: Option<Uuid>) -> ApiResult<()> {
    let due: Vec<(Uuid, Uuid)> = sqlx::query_as(
        "select id, character_id from ground_bags where expires_at <= now() and ($1::uuid is null or character_id = $1)
          for update skip locked",
    )
    .bind(character_id)
    .fetch_all(&mut *tx)
    .await?;
    for (bag, owner) in due {
        let items = items_of(tx, bag).await?;
        journal::log_diff(tx, &Ctx::core(owner, "ground_expired"), &tally(bag, &items), &Tally::new()).await?;
        sqlx::query("delete from ground_bags where id = $1").bind(bag).execute(&mut *tx).await?;
    }
    Ok(())
}

/// `POST /internal/characters/{id}/ground`: drops items into a new ground bag; the bag without them goes along.
pub async fn drop_items(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(d): Json<DropRequest>,
) -> ApiResult<(StatusCode, Json<Bag>)> {
    validate_items(&d.items, false)?;
    d.snapshot.validate()?;
    if d.map.is_empty() || d.map.len() > 120 || !(1..=MAX_TTL).contains(&d.ttl_seconds) {
        return Err(ApiError::bad_request("invalid_ground", "Mapa ou tempo inválido."));
    }
    let mut tx = state.db.begin().await?;
    let ctx = d.snapshot.apply(&mut tx, id).await?;
    let bag = Uuid::new_v4();
    let expires_at = OffsetDateTime::now_utc() + Duration::seconds(d.ttl_seconds);
    sqlx::query(
        "insert into ground_bags (id, character_id, map, pos_x, pos_y, pos_z, expires_at) values ($1, $2, $3, $4, $5, $6, $7)",
    )
    .bind(bag)
    .bind(id)
    .bind(&d.map)
    .bind(d.position.x)
    .bind(d.position.y)
    .bind(d.position.z)
    .bind(expires_at)
    .execute(&mut *tx)
    .await?;
    write_items(&mut tx, bag, &d.items).await?;
    journal::log_diff(&mut tx, &ctx, &Tally::new(), &tally(bag, &d.items)).await?;
    serials::verify(&mut tx, id, "bag", &d.snapshot.serials()).await?;
    serials::verify(&mut tx, id, &place(bag), &serials::serials(d.items.iter().map(|i| &i.props))).await?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(Bag { id: bag, map: d.map, position: d.position, expires_at, items: d.items })))
}

/// `POST /internal/ground/{bag}/take`: takes from the ground bag (what is left stays; empty, it disappears); the bag with
/// the items goes along.
pub async fn take(
    State(state): State<AppState>,
    _server: GameServer,
    Path(bag): Path<Uuid>,
    Json(t): Json<TakeRequest>,
) -> ApiResult<Json<Value>> {
    validate_items(&t.items, true)?;
    t.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    let row: Option<(Uuid, OffsetDateTime)> = sqlx::query_as("select character_id, expires_at from ground_bags where id = $1 for update")
        .bind(bag)
        .fetch_optional(&mut *tx)
        .await?;
    let Some((owner, expires_at)) = row else {
        return Err(ApiError::conflict("ground_gone", "O saco não existe mais."));
    };
    if owner != t.character_id {
        return Err(ApiError::not_found("Saco não encontrado."));
    }
    if expires_at <= OffsetDateTime::now_utc() {
        expire(&mut tx, Some(owner)).await?;
        tx.commit().await?;
        return Err(ApiError::conflict("ground_gone", "O saco venceu."));
    }
    let ctx = t.snapshot.apply(&mut tx, owner).await?;
    let before = items_of(&mut tx, bag).await?;
    if t.items.is_empty() {
        sqlx::query("delete from ground_bags where id = $1").bind(bag).execute(&mut *tx).await?;
    } else {
        write_items(&mut tx, bag, &t.items).await?;
    }
    journal::log_diff(&mut tx, &ctx, &tally(bag, &before), &tally(bag, &t.items)).await?;
    serials::verify(&mut tx, owner, "bag", &t.snapshot.serials()).await?;
    serials::verify(&mut tx, owner, &place(bag), &serials::serials(t.items.iter().map(|i| &i.props))).await?;
    tx.commit().await?;
    Ok(Json(serde_json::json!({ "remaining": t.items.len() })))
}

/// `GET /internal/characters/{id}/ground`: the character's live ground bags (expired ones are deleted here).
pub async fn list(State(state): State<AppState>, _server: GameServer, Path(id): Path<Uuid>) -> ApiResult<Json<Vec<Bag>>> {
    let mut tx = state.db.begin().await?;
    expire(&mut tx, Some(id)).await?;
    let bags: Vec<(Uuid, String, f64, f64, f64, OffsetDateTime)> = sqlx::query_as(
        "select id, map, pos_x, pos_y, pos_z, expires_at from ground_bags where character_id = $1 order by created_at",
    )
    .bind(id)
    .fetch_all(&mut *tx)
    .await?;
    let mut out = Vec::new();
    for (bag, map, x, y, z, expires_at) in bags {
        let items = items_of(&mut tx, bag).await?;
        out.push(Bag { id: bag, map, position: Position { x, y, z }, expires_at, items });
    }
    tx.commit().await?;
    Ok(Json(out))
}
