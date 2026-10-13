//! API do servidor de jogo: resgatar o passe de entrada e salvar o estado do personagem.

use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::{self, GameServer};
use crate::error::{ApiError, ApiResult};
use crate::routes::characters::{character_columns, Character, CharacterRow, CharacterState, ItemInstance};
use crate::routes::serials;
use crate::AppState;

#[derive(Deserialize)]
pub struct RedeemRequest {
    pub ticket: String,
}

#[derive(Serialize)]
pub struct RedeemResponse {
    pub account_id: Uuid,
    pub account_name: String,
    /// Primeira entrada do personagem num servidor de jogo (o servidor dá o kit inicial).
    pub first_join: bool,
    pub character: Character,
}

pub async fn redeem_ticket(State(state): State<AppState>, _server: GameServer, Json(req): Json<RedeemRequest>) -> ApiResult<Json<RedeemResponse>> {
    let mut tx = state.db.begin().await?;
    let redeemed: Option<(Uuid, Uuid)> = sqlx::query_as(
        "update join_tickets set redeemed_at = now()
          where ticket_hash = $1 and redeemed_at is null and expires_at > now()
          returning account_id, character_id",
    )
    .bind(auth::token_hash(req.ticket.trim()))
    .fetch_optional(&mut *tx)
    .await?;
    let Some((account_id, character_id)) = redeemed else {
        return Err(ApiError::new(StatusCode::UNAUTHORIZED, "invalid_ticket", "Passe de entrada inválido ou expirado."));
    };
    let (account_name,): (String,) = sqlx::query_as("select name from accounts where id = $1")
        .bind(account_id)
        .fetch_one(&mut *tx)
        .await?;
    let (previous_seen,): (Option<time::OffsetDateTime>,) =
        sqlx::query_as("select last_seen_at from characters where id = $1 for update")
            .bind(character_id)
            .fetch_one(&mut *tx)
            .await?;
    let row: Option<CharacterRow> = sqlx::query_as(concat!(
        "update characters set last_seen_at = now() where id = $1 and deleted_at is null returning ",
        character_columns!()
    ))
    .bind(character_id)
    .fetch_optional(&mut *tx)
    .await?;
    let mut character: Character = row.ok_or_else(|| ApiError::not_found("Personagem"))?.into();
    let items: Vec<ItemInstance> = sqlx::query_as(
        "select item_id, quantity, container, slot, gear_score, rarity, props from items where character_id = $1 order by container, slot",
    )
    .bind(character_id)
    .fetch_all(&mut *tx)
    .await?;
    character.state.inventory = Some(items);
    tx.commit().await?;
    tracing::info!(account = %account_name, character = %character.name, "entrou no servidor");
    Ok(Json(RedeemResponse { account_id, account_name, first_join: previous_seen.is_none(), character }))
}

pub async fn save_state(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(s): Json<CharacterState>,
) -> ApiResult<StatusCode> {
    if let Some(items) = &s.inventory {
        validate_inventory(items)?;
    }
    if s.coins.is_some_and(|c| c < 0) {
        return Err(ApiError::bad_request("invalid_coins", "Moedas negativas."));
    }
    if s.progression.as_ref().is_some_and(|p| !p.is_object()) {
        return Err(ApiError::bad_request("invalid_progression", "Progressão tem que ser um objeto."));
    }
    let mut tx = state.db.begin().await?;
    let done = sqlx::query(
        "update characters
            set map = $2, pos_x = $3, pos_y = $4, pos_z = $5, yaw = $6, health = $7,
                active_weapon = $8, loadout = $9, mastery = $10, coins = coalesce($11, coins),
                progression = coalesce($12, progression), last_seen_at = now()
          where id = $1 and deleted_at is null",
    )
    .bind(id)
    .bind(&s.map)
    .bind(s.position.map(|p| p.x))
    .bind(s.position.map(|p| p.y))
    .bind(s.position.map(|p| p.z))
    .bind(s.yaw)
    .bind(s.health)
    .bind(&s.active_weapon)
    .bind(&s.loadout)
    .bind(&s.mastery)
    .bind(s.coins)
    .bind(&s.progression)
    .execute(&mut *tx)
    .await?;
    if done.rows_affected() == 0 {
        return Err(ApiError::not_found("Personagem"));
    }
    if let Some(items) = &s.inventory {
        write_inventory(&mut tx, id, items).await?;
        serials::verify(&mut tx, id, "bag", &serials::serials(items.iter().map(|i| &i.props))).await?;
    }
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

/// Bolsa e equipado: item válido e sem slot repetido.
pub(crate) fn validate_inventory(items: &[ItemInstance]) -> ApiResult<()> {
    let mut seen = std::collections::HashSet::new();
    for it in items {
        it.validate().map_err(|e| ApiError::bad_request("invalid_inventory", e))?;
        if !seen.insert((it.container.as_str(), it.slot)) {
            return Err(ApiError::bad_request("invalid_inventory", format!("slot repetido: {} {}", it.container, it.slot)));
        }
    }
    Ok(())
}

/// Substitui o inventário inteiro do personagem (dentro da transação de quem chama).
pub(crate) async fn write_inventory(tx: &mut sqlx::PgConnection, id: Uuid, items: &[ItemInstance]) -> ApiResult<()> {
    sqlx::query("delete from items where character_id = $1").bind(id).execute(&mut *tx).await?;
    for it in items {
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
    }
    Ok(())
}
