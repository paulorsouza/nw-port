//! Houses: purchase. House chests add capacity to the shared Storage (migration 0010; the game server sums it). Cost and minimum standing come from
//! `javelindata_housetypes`, checked by the game server. Decoration and interior are left for later.

use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{is_unique_violation, ApiError, ApiResult};
use crate::routes::storage::{is_territory, Snapshot};
use crate::AppState;

#[derive(Serialize, Debug, sqlx::FromRow)]
pub struct House {
    pub id: Uuid,
    pub territory: String,
    pub address: String,
    pub house_type: String,
    pub price: i64,
    #[serde(with = "time::serde::rfc3339")]
    pub purchased_at: OffsetDateTime,
}

#[derive(Deserialize, Debug)]
pub struct BuyHouse {
    pub territory: String,
    pub address: String,
    pub house_type: String,
    pub price: i64,
    /// Bag and coins after paying.
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

pub async fn list(State(state): State<AppState>, _server: GameServer, Path(id): Path<Uuid>) -> ApiResult<Json<Vec<House>>> {
    let houses: Vec<House> = sqlx::query_as(
        "select id, territory, address, house_type, price, purchased_at from houses where character_id = $1 order by purchased_at",
    )
    .bind(id)
    .fetch_all(&state.db)
    .await?;
    Ok(Json(houses))
}

pub async fn buy(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(b): Json<BuyHouse>,
) -> ApiResult<(StatusCode, Json<House>)> {
    if !is_territory(&b.territory) || b.address.is_empty() || b.address.len() > 120 || b.house_type.is_empty() || b.price < 0 {
        return Err(ApiError::bad_request("invalid_house", "Território, endereço, tipo ou preço inválidos."));
    }
    b.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    b.snapshot.apply(&mut tx, id).await?;
    crate::routes::serials::verify(&mut tx, id, "bag", &b.snapshot.serials()).await?;
    let house: House = sqlx::query_as(
        "insert into houses (id, character_id, territory, address, house_type, price) values ($1, $2, $3, $4, $5, $6)
         returning id, territory, address, house_type, price, purchased_at",
    )
    .bind(Uuid::new_v4())
    .bind(id)
    .bind(&b.territory)
    .bind(&b.address)
    .bind(&b.house_type)
    .bind(b.price)
    .fetch_one(&mut *tx)
    .await
    .map_err(|e| {
        if is_unique_violation(&e) {
            ApiError::conflict("house_owned", "Já tem uma casa neste território.")
        } else {
            e.into()
        }
    })?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(house)))
}
