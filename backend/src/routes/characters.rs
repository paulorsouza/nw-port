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

/// What the game server saves and reloads.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct CharacterState {
    pub map: String,
    pub position: Option<Position>,
    pub yaw: f32,
    pub health: Option<f32>,
    pub active_weapon: String,
    pub loadout: Value,
    pub mastery: Value,
    /// Missing on save = keeps what is in the database.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub coins: Option<i64>,
    /// Whole inventory (bag and equipped). Missing = keeps; present = replaces.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub inventory: Option<Vec<ItemInstance>>,
    /// Level, XP, attributes and trade skills (game server object). Missing = keeps.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub progression: Option<Value>,
    /// Shared Storage capacity (kg), summed by the game server; missing = keeps.
    #[serde(default, skip_serializing)]
    pub storage_capacity: Option<f32>,
    /// Only the bag/equipped slots that changed (instead of `inventory`): new item, or null to clear.
    #[serde(default, skip_serializing)]
    pub inventory_patch: Option<Vec<SlotPatch>>,
    /// Session, sequence number and reason of the save (game server; journal.rs).
    #[serde(default, skip_serializing)]
    pub session: Option<i64>,
    #[serde(default, skip_serializing)]
    pub seq: Option<i64>,
    #[serde(default, skip_serializing)]
    pub reason: Option<String>,
    /// Crafting tax at a city's stations (governance), in the same transaction.
    #[serde(default, skip_serializing)]
    pub city_tax: Option<crate::routes::city::CityTax>,
}

/// A character item (game ItemID, see Data/Items/items.*.json in the Unreal project).
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, sqlx::FromRow)]
pub struct ItemInstance {
    pub item_id: String,
    pub quantity: i32,
    /// "bag" or "equip".
    pub container: String,
    pub slot: i32,
    #[serde(default)]
    pub gear_score: i32,
    #[serde(default)]
    pub rarity: i16,
    /// Attribute, quality, perks, gem, binding and signature (docs/game/items.md); the database does not parse it.
    #[serde(default = "empty_props")]
    pub props: Value,
}

pub(crate) const MAX_BAG_SLOTS: i32 = 500;
pub(crate) const MAX_PROPS_BYTES: usize = 2048;

pub(crate) fn empty_props() -> Value {
    Value::Object(Default::default())
}

/// `props` must be a small object.
pub(crate) fn validate_props(props: &Value, item_id: &str) -> Result<(), String> {
    if !props.is_object() {
        return Err(format!("props de {item_id} tem que ser um objeto"));
    }
    if props.to_string().len() > MAX_PROPS_BYTES {
        return Err(format!("props de {item_id} grande demais"));
    }
    Ok(())
}

/// Item bound to the character (cannot go to the Trading Post).
pub(crate) fn is_bound(props: &Value) -> bool {
    props.get("bound").and_then(Value::as_bool).unwrap_or(false)
}
pub(crate) const MAX_EQUIP_SLOTS: i32 = 16;

/// A bag or equipped slot: the item that goes in it, or none.
#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct SlotPatch {
    pub container: String,
    pub slot: i32,
    pub item: Option<PatchItem>,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct PatchItem {
    pub item_id: String,
    pub quantity: i32,
    #[serde(default)]
    pub gear_score: i32,
    #[serde(default)]
    pub rarity: i16,
    #[serde(default = "empty_props")]
    pub props: Value,
}

impl SlotPatch {
    /// The full item for the slot (to validate and store), or None when clearing.
    pub(crate) fn instance(&self) -> Option<ItemInstance> {
        self.item.as_ref().map(|i| ItemInstance {
            item_id: i.item_id.clone(),
            quantity: i.quantity,
            container: self.container.clone(),
            slot: self.slot,
            gear_score: i.gear_score,
            rarity: i.rarity,
            props: i.props.clone(),
        })
    }

    pub(crate) fn validate(&self) -> Result<(), String> {
        match self.instance() {
            Some(i) => i.validate(),
            None => ItemInstance { item_id: "x".into(), quantity: 1, container: self.container.clone(), slot: self.slot, gear_score: 0, rarity: 0, props: empty_props() }.validate(),
        }
    }
}

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
        validate_props(&self.props, &self.item_id)
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

/// `CharacterRow` columns, as a macro so they fit in `concat!` (sqlx only accepts static SQL).
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
                storage_capacity: None,
                inventory_patch: None,
                city_tax: None,
                session: None,
                seq: None,
                reason: None,
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
    // Locks the account so the count does not race with another concurrent creation.
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
    tracing::info!(account = %session.name, character = %name, "character created");
    Ok((StatusCode::CREATED, Json(row.into())))
}

pub async fn remove(State(state): State<AppState>, session: Session, Path(id): Path<Uuid>) -> ApiResult<StatusCode> {
    let done = sqlx::query("update characters set deleted_at = now() where id = $1 and account_id = $2 and deleted_at is null")
        .bind(id)
        .bind(session.account_id)
        .execute(&state.db)
        .await?;
    if done.rows_affected() == 0 {
        return Err(ApiError::not_found("Personagem não encontrado."));
    }
    Ok(StatusCode::NO_CONTENT)
}

/// Single-use ticket for the game server; goes in the client's connect URL (`?ticket=`).
pub async fn join(State(state): State<AppState>, session: Session, Path(id): Path<Uuid>) -> ApiResult<Json<JoinResponse>> {
    let owned: Option<(Uuid,)> =
        sqlx::query_as("select id from characters where id = $1 and account_id = $2 and deleted_at is null")
            .bind(id)
            .bind(session.account_id)
            .fetch_optional(&state.db)
            .await?;
    if owned.is_none() {
        return Err(ApiError::not_found("Personagem não encontrado."));
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
