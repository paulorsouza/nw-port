use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use time::{Duration, OffsetDateTime};
use uuid::Uuid;

use crate::auth::{self, Session};
use crate::error::{is_unique_violation, ApiError, ApiResult};
use crate::AppState;

const MAX_CHARACTERS: i64 = 3;
const TICKET_SECONDS: i64 = 60;

#[derive(Serialize, Deserialize, Clone, Copy, Debug, PartialEq)]
pub struct Position {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

/// O que o servidor de jogo salva e recarrega.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct CharacterState {
    pub map: String,
    pub position: Option<Position>,
    pub yaw: f32,
    pub health: Option<f32>,
    pub active_weapon: String,
    pub loadout: Value,
    pub mastery: Value,
    /// Ausente no salvamento = mantém o que está no banco.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub coins: Option<i64>,
    /// Inventário inteiro (bolsa e equipado). Ausente = mantém; presente = substitui.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub inventory: Option<Vec<ItemInstance>>,
    /// Nível, XP, atributos e ofícios (objeto do servidor de jogo). Ausente = mantém.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub progression: Option<Value>,
}

/// Um item do personagem (ItemID do jogo, ver Data/Items/items.*.json no projeto Unreal).
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, sqlx::FromRow)]
pub struct ItemInstance {
    pub item_id: String,
    pub quantity: i32,
    /// "bag" ou "equip".
    pub container: String,
    pub slot: i32,
    #[serde(default)]
    pub gear_score: i32,
    #[serde(default)]
    pub rarity: i16,
}

pub(crate) const MAX_BAG_SLOTS: i32 = 200;
pub(crate) const MAX_EQUIP_SLOTS: i32 = 16;

impl ItemInstance {
    pub(crate) fn validate(&self) -> Result<(), String> {
        if self.item_id.is_empty() || self.item_id.len() > 80 {
            return Err(format!("item_id inválido: {:?}", self.item_id));
        }
        if self.quantity <= 0 {
            return Err(format!("quantidade inválida em {}", self.item_id));
        }
        let max = match self.container.as_str() {
            "bag" => MAX_BAG_SLOTS,
            "equip" => MAX_EQUIP_SLOTS,
            other => return Err(format!("contêiner inválido: {other}")),
        };
        if !(0..max).contains(&self.slot) {
            return Err(format!("slot {} fora de {}", self.slot, self.container));
        }
        Ok(())
    }
}

#[derive(Serialize, Debug)]
pub struct Character {
    pub id: Uuid,
    pub name: String,
    #[serde(with = "time::serde::rfc3339")]
    pub created_at: OffsetDateTime,
    #[serde(with = "time::serde::rfc3339::option")]
    pub last_seen_at: Option<OffsetDateTime>,
    #[serde(flatten)]
    pub state: CharacterState,
}

#[derive(sqlx::FromRow)]
pub(crate) struct CharacterRow {
    id: Uuid,
    name: String,
    created_at: OffsetDateTime,
    last_seen_at: Option<OffsetDateTime>,
    map: String,
    pos_x: Option<f64>,
    pos_y: Option<f64>,
    pos_z: Option<f64>,
    yaw: f32,
    health: Option<f32>,
    active_weapon: String,
    loadout: Value,
    mastery: Value,
    coins: i64,
    progression: Value,
}

/// Colunas de `CharacterRow`, em macro para entrar em `concat!` (o sqlx só aceita SQL estático).
macro_rules! character_columns {
    () => {
        "id, name, created_at, last_seen_at, map, pos_x, pos_y, pos_z, yaw, health, active_weapon, loadout, mastery, coins, progression"
    };
}
pub(crate) use character_columns;

impl From<CharacterRow> for Character {
    fn from(r: CharacterRow) -> Self {
        let position = match (r.pos_x, r.pos_y, r.pos_z) {
            (Some(x), Some(y), Some(z)) => Some(Position { x, y, z }),
            _ => None,
        };
        Character {
            id: r.id,
            name: r.name,
            created_at: r.created_at,
            last_seen_at: r.last_seen_at,
            state: CharacterState {
                map: r.map,
                position,
                yaw: r.yaw,
                health: r.health,
                active_weapon: r.active_weapon,
                loadout: r.loadout,
                mastery: r.mastery,
                coins: Some(r.coins),
                inventory: None,
                progression: Some(r.progression),
            },
        }
    }
}

#[derive(Deserialize)]
pub struct CreateRequest {
    pub name: String,
}

#[derive(Serialize)]
pub struct JoinResponse {
    pub ticket: String,
    #[serde(with = "time::serde::rfc3339")]
    pub expires_at: OffsetDateTime,
}

fn validate_character_name(name: &str) -> ApiResult<()> {
    let ok = (3..=16).contains(&name.chars().count()) && name.chars().all(char::is_alphabetic);
    if ok {
        Ok(())
    } else {
        Err(ApiError::bad_request("invalid_name", "Nome de personagem: 3 a 16 letras."))
    }
}

pub async fn list_for(state: &AppState, account_id: Uuid) -> ApiResult<Vec<Character>> {
    let rows: Vec<CharacterRow> = sqlx::query_as(concat!(
        "select ",
        character_columns!(),
        " from characters where account_id = $1 and deleted_at is null order by created_at"
    ))
    .bind(account_id)
    .fetch_all(&state.db)
    .await?;
    Ok(rows.into_iter().map(Character::from).collect())
}

pub async fn list(State(state): State<AppState>, session: Session) -> ApiResult<Json<Vec<Character>>> {
    Ok(Json(list_for(&state, session.account_id).await?))
}

pub async fn create(State(state): State<AppState>, session: Session, Json(req): Json<CreateRequest>) -> ApiResult<(StatusCode, Json<Character>)> {
    let name = req.name.trim().to_string();
    validate_character_name(&name)?;

    let mut tx = state.db.begin().await?;
    // Trava a conta para a contagem não correr com outra criação simultânea.
    sqlx::query("select 1 from accounts where id = $1 for update")
        .bind(session.account_id)
        .execute(&mut *tx)
        .await?;
    let (count,): (i64,) = sqlx::query_as("select count(*) from characters where account_id = $1 and deleted_at is null")
        .bind(session.account_id)
        .fetch_one(&mut *tx)
        .await?;
    if count >= MAX_CHARACTERS {
        return Err(ApiError::conflict("too_many_characters", format!("Limite de {MAX_CHARACTERS} personagens por conta.")));
    }
    let inserted: Result<CharacterRow, _> = sqlx::query_as(concat!(
        "insert into characters (id, account_id, name) values ($1, $2, $3) returning ",
        character_columns!()
    ))
    .bind(Uuid::new_v4())
    .bind(session.account_id)
    .bind(&name)
    .fetch_one(&mut *tx)
    .await;
    let row = match inserted {
        Err(e) if is_unique_violation(&e) => return Err(ApiError::conflict("name_taken", "Esse nome de personagem já existe.")),
        other => other?,
    };
    tx.commit().await?;
    tracing::info!(account = %session.name, character = %name, "personagem criado");
    Ok((StatusCode::CREATED, Json(row.into())))
}

pub async fn remove(State(state): State<AppState>, session: Session, Path(id): Path<Uuid>) -> ApiResult<StatusCode> {
    let done = sqlx::query("update characters set deleted_at = now() where id = $1 and account_id = $2 and deleted_at is null")
        .bind(id)
        .bind(session.account_id)
        .execute(&state.db)
        .await?;
    if done.rows_affected() == 0 {
        return Err(ApiError::not_found("Personagem"));
    }
    Ok(StatusCode::NO_CONTENT)
}

/// Passe de uso único para o servidor de jogo; vai na URL de conexão do cliente (`?ticket=`).
pub async fn join(State(state): State<AppState>, session: Session, Path(id): Path<Uuid>) -> ApiResult<Json<JoinResponse>> {
    let owned: Option<(Uuid,)> =
        sqlx::query_as("select id from characters where id = $1 and account_id = $2 and deleted_at is null")
            .bind(id)
            .bind(session.account_id)
            .fetch_optional(&state.db)
            .await?;
    if owned.is_none() {
        return Err(ApiError::not_found("Personagem"));
    }
    let ticket = auth::new_token();
    let expires_at = OffsetDateTime::now_utc() + Duration::seconds(TICKET_SECONDS);
    sqlx::query("insert into join_tickets (ticket_hash, account_id, character_id, expires_at) values ($1, $2, $3, $4)")
        .bind(auth::token_hash(&ticket))
        .bind(session.account_id)
        .bind(id)
        .bind(expires_at)
        .execute(&state.db)
        .await?;
    Ok(Json(JoinResponse { ticket, expires_at }))
}
