//! Save ordering and item log (migration 0011).
//!
//! - `WriteSeq`: session and save number that the game server sends on every write of bag, coins or
//!   Storage. `claim` only lets through the current session with a `seq` greater than the last one (409 `stale_write`;
//!   the same `seq` again is 409 `duplicate_write`, which the server treats as already saved).
//! - `item_log`: one row per item or coin difference, in the same transaction as the save.

use std::collections::HashMap;

use axum::extract::{Query, State};
use axum::Json;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use time::OffsetDateTime;
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{ApiError, ApiResult};
use crate::AppState;

/// Session and save number, plus the reason for the log.
#[derive(Deserialize, Debug, Clone, Default)]
pub struct WriteSeq {
    pub session: Option<i64>,
    pub seq: Option<i64>,
    #[serde(default)]
    pub reason: Option<String>,
}

/// Who saved, for the log rows.
#[derive(Debug, Clone)]
pub(crate) struct Ctx {
    pub character_id: Uuid,
    pub session: Option<i64>,
    pub seq: Option<i64>,
    pub reason: String,
}

impl Ctx {
    /// Action by nw-core itself (market delivery, refund): no session.
    pub(crate) fn core(character_id: Uuid, reason: &str) -> Self {
        Self { character_id, session: None, seq: None, reason: reason.to_string() }
    }
}

const MAX_REASON: usize = 80;

/// Checks and reserves the save's `seq` (locks the character row until the end of the transaction).
pub(crate) async fn claim(tx: &mut sqlx::PgConnection, character_id: Uuid, w: &WriteSeq) -> ApiResult<Ctx> {
    let (Some(session), Some(seq)) = (w.session, w.seq) else {
        return Err(ApiError::bad_request("missing_seq", "Gravação sem sessão e número."));
    };
    let reason = w.reason.clone().unwrap_or_else(|| "save".into());
    if reason.is_empty() || reason.len() > MAX_REASON {
        return Err(ApiError::bad_request("invalid_reason", "Motivo inválido."));
    }
    let row: Option<(i64, i64)> = sqlx::query_as("select session, save_seq from characters where id = $1 and deleted_at is null for update")
        .bind(character_id)
        .fetch_optional(&mut *tx)
        .await?;
    let (current, last) = row.ok_or_else(|| ApiError::not_found("Personagem não encontrado."))?;
    if session != current || seq < last {
        return Err(ApiError::conflict("stale_write", format!("Gravação velha (sessão {session}/{current}, número {seq}/{last}).")));
    }
    if seq == last {
        return Err(ApiError::conflict("duplicate_write", "Esta gravação já foi aplicada."));
    }
    sqlx::query("update characters set save_seq = $2 where id = $1")
        .bind(character_id)
        .bind(seq)
        .execute(&mut *tx)
        .await?;
    Ok(Ctx { character_id, session: Some(session), seq: Some(seq), reason })
}

/// Opens a new session (ticket redeem); returns its number.
pub(crate) async fn new_session(tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<i64> {
    let (session,): (i64,) = sqlx::query_as("update characters set session = session + 1, save_seq = 0 where id = $1 returning session")
        .bind(character_id)
        .fetch_one(&mut *tx)
        .await?;
    Ok(session)
}

/// One log row.
#[allow(clippy::too_many_arguments)]
pub(crate) async fn log(
    tx: &mut sqlx::PgConnection,
    ctx: &Ctx,
    place: &str,
    item_id: Option<&str>,
    quantity: i64,
    gear_score: i32,
    rarity: i16,
    props: &Value,
) -> ApiResult<()> {
    if quantity == 0 {
        return Ok(());
    }
    sqlx::query(
        "insert into item_log (character_id, session, seq, reason, place, item_id, quantity, gear_score, rarity, props)
         values ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)",
    )
    .bind(ctx.character_id)
    .bind(ctx.session)
    .bind(ctx.seq)
    .bind(&ctx.reason)
    .bind(place)
    .bind(item_id)
    .bind(quantity)
    .bind(gear_score)
    .bind(rarity)
    .bind(props)
    .execute(&mut *tx)
    .await?;
    Ok(())
}

/// Stack for comparing before and after: place, item, GS, rarity and props (the slot does not matter).
#[derive(Hash, PartialEq, Eq, Clone)]
pub(crate) struct Key {
    pub place: String,
    pub item_id: String,
    pub gear_score: i32,
    pub rarity: i16,
    pub props: String,
}

pub(crate) type Tally = HashMap<Key, (i64, Value)>;

pub(crate) fn add(t: &mut Tally, place: &str, item_id: &str, quantity: i64, gear_score: i32, rarity: i16, props: &Value) {
    let key = Key { place: place.to_string(), item_id: item_id.to_string(), gear_score, rarity, props: props.to_string() };
    t.entry(key).or_insert((0, props.clone())).0 += quantity;
}

/// Writes the difference between before and after to the log.
pub(crate) async fn log_diff(tx: &mut sqlx::PgConnection, ctx: &Ctx, before: &Tally, after: &Tally) -> ApiResult<()> {
    let mut keys: Vec<&Key> = before.keys().chain(after.keys().filter(|k| !before.contains_key(*k))).collect();
    keys.sort_by(|a, b| (&a.place, &a.item_id).cmp(&(&b.place, &b.item_id)));
    for k in keys {
        let was = before.get(k).map_or(0, |v| v.0);
        let now = after.get(k).map_or(0, |v| v.0);
        let props = after.get(k).or_else(|| before.get(k)).map(|v| v.1.clone()).unwrap_or_else(|| serde_json::json!({}));
        log(tx, ctx, &k.place, Some(&k.item_id), now - was, k.gear_score, k.rarity, &props).await?;
    }
    Ok(())
}

/// Bag and equipped from a list (what is about to be saved).
pub(crate) fn tally_items(items: &[crate::routes::characters::ItemInstance]) -> Tally {
    let mut t = Tally::new();
    for it in items {
        add(&mut t, &it.container, &it.item_id, it.quantity as i64, it.gear_score, it.rarity, &it.props);
    }
    t
}

/// Saves the whole bag and the coins, with the difference in the log.
pub(crate) async fn write_bag(
    tx: &mut sqlx::PgConnection,
    ctx: &Ctx,
    items: Option<&[crate::routes::characters::ItemInstance]>,
    coins: Option<i64>,
) -> ApiResult<()> {
    let id = ctx.character_id;
    if let Some(mut c) = coins {
        let was = coins_of(tx, id).await?;
        // Coin cap: the excess is not added and goes to the log (the game server already cuts it; this is the guarantee).
        let max = crate::routes::city::RULES.moedas_max;
        if c > max && c > was {
            let kept = max.max(was);
            log(tx, ctx, "coins_cap", None, -(c - kept), 0, 0, &serde_json::json!({})).await?;
            c = kept;
        }
        sqlx::query("update characters set coins = $2 where id = $1").bind(id).bind(c).execute(&mut *tx).await?;
        log(tx, ctx, "coins", None, c - was, 0, 0, &serde_json::json!({})).await?;
    }
    if let Some(items) = items {
        let before = bag_tally(tx, id).await?;
        crate::routes::internal::write_inventory(tx, id, items).await?;
        log_diff(tx, ctx, &before, &tally_items(items)).await?;
    }
    Ok(())
}

/// Applies only the slots that changed (bag and equipped), with the difference in the log.
pub(crate) async fn patch_bag(tx: &mut sqlx::PgConnection, ctx: &Ctx, patches: &[crate::routes::characters::SlotPatch]) -> ApiResult<()> {
    let id = ctx.character_id;
    let mut before = Tally::new();
    let mut after = Tally::new();
    for p in patches {
        let old: Option<(String, i32, i32, i16, Value)> = sqlx::query_as(
            "delete from items where character_id = $1 and container = $2 and slot = $3
             returning item_id, quantity, gear_score, rarity, props",
        )
        .bind(id)
        .bind(&p.container)
        .bind(p.slot)
        .fetch_optional(&mut *tx)
        .await?;
        if let Some((item, q, gs, r, props)) = old {
            add(&mut before, &p.container, &item, q as i64, gs, r, &props);
        }
        if let Some(it) = p.instance() {
            sqlx::query(
                "insert into items (id, character_id, item_id, quantity, container, slot, gear_score, rarity, props)
                 values ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
            )
            .bind(Uuid::new_v4())
            .bind(id)
            .bind(&it.item_id)
            .bind(it.quantity)
            .bind(&it.container)
            .bind(it.slot)
            .bind(it.gear_score)
            .bind(it.rarity)
            .bind(&it.props)
            .execute(&mut *tx)
            .await?;
            add(&mut after, &it.container, &it.item_id, it.quantity as i64, it.gear_score, it.rarity, &it.props);
        }
    }
    log_diff(tx, ctx, &before, &after).await
}

/// Bag and equipped as currently stored.
pub(crate) async fn bag_tally(tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<Tally> {
    let rows: Vec<(String, String, i32, i32, i16, Value)> =
        sqlx::query_as("select container, item_id, quantity, gear_score, rarity, props from items where character_id = $1")
            .bind(character_id)
            .fetch_all(&mut *tx)
            .await?;
    let mut t = Tally::new();
    for (place, item, q, gs, r, props) in rows {
        add(&mut t, &place, &item, q as i64, gs, r, &props);
    }
    Ok(t)
}

/// Shared Storage as currently stored (place `storage:<tab>`).
pub(crate) async fn storage_tally(tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<Tally> {
    let rows: Vec<(i16, String, i32, i32, i16, Value)> = sqlx::query_as(
        "select tab, item_id, quantity, gear_score, rarity, props from storage_items where character_id = $1 and location = 'shared'",
    )
    .bind(character_id)
    .fetch_all(&mut *tx)
    .await?;
    let mut t = Tally::new();
    for (tab, item, q, gs, r, props) in rows {
        add(&mut t, &format!("storage:{tab}"), &item, q as i64, gs, r, &props);
    }
    Ok(t)
}

/// Coins as currently stored.
pub(crate) async fn coins_of(tx: &mut sqlx::PgConnection, character_id: Uuid) -> ApiResult<i64> {
    let (c,): (i64,) = sqlx::query_as("select coins from characters where id = $1")
        .bind(character_id)
        .fetch_one(&mut *tx)
        .await?;
    Ok(c)
}

// ---------------------------------------------------------------- query

#[derive(Deserialize, Debug)]
pub struct LogQuery {
    pub character_id: Option<Uuid>,
    /// Character name (case-insensitive).
    pub character: Option<String>,
    pub item_id: Option<String>,
    pub serial: Option<String>,
    #[serde(default, with = "time::serde::rfc3339::option")]
    pub since: Option<OffsetDateTime>,
    pub limit: Option<i64>,
}

#[derive(Serialize, Debug, sqlx::FromRow)]
pub struct LogRow {
    pub id: i64,
    #[serde(with = "time::serde::rfc3339")]
    pub at: OffsetDateTime,
    pub character_id: Uuid,
    pub session: Option<i64>,
    pub seq: Option<i64>,
    pub reason: String,
    pub place: String,
    pub item_id: Option<String>,
    pub quantity: i64,
    pub gear_score: i32,
    pub rarity: i16,
    pub props: Value,
}

/// Coins the game server did not grant because of the cap (`coins_cap`): record only.
#[derive(Deserialize)]
pub struct CoinsCap {
    pub amount: i64,
    #[serde(default)]
    pub reason: Option<String>,
}

pub async fn coins_cap(
    State(state): State<AppState>,
    _server: GameServer,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
    axum::Json(c): axum::Json<CoinsCap>,
) -> ApiResult<axum::http::StatusCode> {
    if c.amount <= 0 {
        return Err(ApiError::bad_request("invalid_amount", "Valor inválido."));
    }
    let reason = c.reason.unwrap_or_else(|| "coins_cap".into());
    let mut tx = state.db.begin().await?;
    log(&mut tx, &Ctx::core(id, &reason.chars().take(MAX_REASON).collect::<String>()), "coins_cap", None, -c.amount, 0, 0, &serde_json::json!({})).await?;
    tx.commit().await?;
    Ok(axum::http::StatusCode::NO_CONTENT)
}

/// `GET /internal/item-log`: filters by character, item, serial number and date; newest first.
pub async fn list(State(state): State<AppState>, _server: GameServer, Query(q): Query<LogQuery>) -> ApiResult<Json<Vec<LogRow>>> {
    let limit = q.limit.unwrap_or(200).clamp(1, 5000);
    let rows: Vec<LogRow> = sqlx::query_as(
        "select id, at, character_id, session, seq, reason, place, item_id, quantity, gear_score, rarity, props from item_log
          where ($1::uuid is null or character_id = $1) and ($2::text is null or item_id = $2)
            and ($3::text is null or serial = $3) and ($4::timestamptz is null or at >= $4)
            and ($6::text is null or character_id in (select id from characters where lower(name) = lower($6)))
          order by id desc limit $5",
    )
    .bind(q.character_id)
    .bind(&q.item_id)
    .bind(&q.serial)
    .bind(q.since)
    .bind(limit)
    .bind(&q.character)
    .fetch_all(&state.db)
    .await?;
    Ok(Json(rows))
}
