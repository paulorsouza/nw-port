//! API do servidor de jogo: resgatar o passe de entrada e salvar o estado do personagem.

use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::{self, GameServer};
use crate::error::{ApiError, ApiResult};
use crate::routes::characters::{character_columns, Character, CharacterRow, CharacterState, ItemInstance};
use crate::routes::journal::{self, WriteSeq};
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
    /// Sessão aberta por este resgate: toda gravação do personagem leva este número e um `seq` crescente.
    pub session: i64,
    /// Conta admin (`nw-core admin <conta>`): só ela usa os comandos de teste (F10) no servidor.
    pub is_admin: bool,
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
    let (account_name, is_admin): (String, bool) = sqlx::query_as("select name, is_admin from accounts where id = $1")
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
    let session = journal::new_session(&mut tx, character_id).await?;
    tx.commit().await?;
    tracing::info!(account = %account_name, character = %character.name, session, "entrou no servidor");
    Ok(Json(RedeemResponse { account_id, account_name, first_join: previous_seen.is_none(), character, session, is_admin }))
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
    if let Some(patches) = &s.inventory_patch {
        if s.inventory.is_some() {
            return Err(ApiError::bad_request("invalid_inventory", "inventory e inventory_patch juntos."));
        }
        let mut seen = std::collections::HashSet::new();
        for p in patches {
            p.validate().map_err(|e| ApiError::bad_request("invalid_inventory", e))?;
            if !seen.insert((p.container.as_str(), p.slot)) {
                return Err(ApiError::bad_request("invalid_inventory", format!("slot repetido: {} {}", p.container, p.slot)));
            }
        }
    }
    if s.coins.is_some_and(|c| c < 0) {
        return Err(ApiError::bad_request("invalid_coins", "Moedas negativas."));
    }
    if s.progression.as_ref().is_some_and(|p| !p.is_object()) {
        return Err(ApiError::bad_request("invalid_progression", "Progressão tem que ser um objeto."));
    }
    if s.storage_capacity.is_some_and(|c| !c.is_finite() || c < 0.0) {
        return Err(ApiError::bad_request("invalid_storage", "Capacidade inválida."));
    }
    let mut tx = state.db.begin().await?;
    let ctx = journal::claim(&mut tx, id, &WriteSeq { session: s.session, seq: s.seq, reason: s.reason.clone() }).await?;
    journal::write_bag(&mut tx, &ctx, None, s.coins).await?;
    let done = sqlx::query(
        "update characters
            set map = $2, pos_x = $3, pos_y = $4, pos_z = $5, yaw = $6, health = $7,
                active_weapon = $8, loadout = $9, mastery = $10,
                progression = coalesce($11, progression), storage_capacity = coalesce($12, storage_capacity),
                last_seen_at = now()
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
    .bind(&s.progression)
    .bind(s.storage_capacity)
    .execute(&mut *tx)
    .await?;
    if done.rows_affected() == 0 {
        return Err(ApiError::not_found("Personagem"));
    }
    if let Some(patches) = &s.inventory_patch {
        journal::patch_bag(&mut tx, &ctx, patches).await?;
        let changed: Vec<_> = patches.iter().filter_map(|p| p.instance()).collect();
        serials::verify(&mut tx, id, "bag", &serials::serials(changed.iter().map(|i| &i.props))).await?;
    }
    if let Some(items) = &s.inventory {
        journal::write_bag(&mut tx, &ctx, Some(items), None).await?;
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
