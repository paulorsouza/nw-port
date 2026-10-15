//! NPC rewards: what the character already claimed and the claim itself (starter items once; weekly reward per period).
//! The game server decides and rolls the items; this module guarantees nothing is claimed twice, stored in the same
//! transaction as the bag.

use axum::extract::{Path, Query, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{is_unique_violation, ApiError, ApiResult};
use crate::routes::storage::Snapshot;
use crate::AppState;

#[derive(Serialize, Debug, sqlx::FromRow)]
pub struct Claim {
    pub npc: String,
    pub kind: String,
    pub period: String,
    #[serde(with = "time::serde::rfc3339")]
    pub claimed_at: OffsetDateTime,
}

#[derive(Serialize, Debug)]
pub struct ClaimList {
    pub claims: Vec<Claim>,
}

#[derive(Deserialize, Debug)]
pub struct ListQuery {
    /// Current weekly period: returns the 'once' claims and the ones for this period.
    pub period: Option<String>,
}

#[derive(Deserialize, Debug)]
pub struct NewClaim {
    pub npc: String,
    pub kind: String,
    pub period: String,
    /// Bag and coins with the items already added.
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

fn valid_npc(npc: &str) -> bool {
    !npc.is_empty() && npc.len() <= 80 && npc.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_')
}

fn valid_period(kind: &str, period: &str) -> bool {
    match kind {
        "starter" => period == "once",
        "weekly" => time::Date::parse(period, time::macros::format_description!("[year]-[month]-[day]")).is_ok(),
        _ => false,
    }
}

pub async fn list(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Query(q): Query<ListQuery>,
) -> ApiResult<Json<ClaimList>> {
    let claims: Vec<Claim> = sqlx::query_as(
        "select npc, kind, period, claimed_at from npc_rewards
          where character_id = $1 and (period = 'once' or period = $2) order by claimed_at",
    )
    .bind(id)
    .bind(q.period.unwrap_or_default())
    .fetch_all(&state.db)
    .await?;
    Ok(Json(ClaimList { claims }))
}

pub async fn claim(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(c): Json<NewClaim>,
) -> ApiResult<(StatusCode, Json<Claim>)> {
    if !valid_npc(&c.npc) || !valid_period(&c.kind, &c.period) {
        return Err(ApiError::bad_request("invalid_claim", "NPC, tipo ou período inválidos."));
    }
    c.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    let claim: Claim = sqlx::query_as(
        "insert into npc_rewards (character_id, npc, kind, period) values ($1, $2, $3, $4)
         returning npc, kind, period, claimed_at",
    )
    .bind(id)
    .bind(&c.npc)
    .bind(&c.kind)
    .bind(&c.period)
    .fetch_one(&mut *tx)
    .await
    .map_err(|e| {
        if is_unique_violation(&e) {
            ApiError::conflict("already_claimed", "Já pegou neste período.")
        } else if matches!(&e, sqlx::Error::Database(db) if db.code().as_deref() == Some("23503")) {
            ApiError::not_found("Personagem não encontrado.")
        } else {
            e.into()
        }
    })?;
    c.snapshot.apply(&mut tx, id).await?;
    crate::routes::serials::verify(&mut tx, id, "bag", &c.snapshot.serials()).await?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(claim)))
}
