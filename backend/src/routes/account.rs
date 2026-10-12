use axum::extract::State;
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use time::{Duration, OffsetDateTime};
use uuid::Uuid;

use crate::auth::{self, Session};
use crate::error::{is_unique_violation, ApiError, ApiResult};
use crate::routes::characters::{list_for, Character};
use crate::AppState;

const SESSION_DAYS: i64 = 30;
const INVITE_DAYS: i64 = 7;

#[derive(Deserialize)]
pub struct RegisterRequest {
    pub invite: String,
    pub name: String,
    pub password: String,
}

#[derive(Deserialize)]
pub struct LoginRequest {
    pub name: String,
    pub password: String,
}

#[derive(Serialize)]
pub struct Account {
    pub id: Uuid,
    pub name: String,
    pub is_admin: bool,
}

#[derive(Serialize)]
pub struct SessionResponse {
    pub token: String,
    #[serde(with = "time::serde::rfc3339")]
    pub expires_at: OffsetDateTime,
    pub account: Account,
}

#[derive(Serialize)]
pub struct MeResponse {
    pub account: Account,
    pub characters: Vec<Character>,
}

#[derive(Serialize)]
pub struct InviteResponse {
    pub code: String,
    #[serde(with = "time::serde::rfc3339")]
    pub expires_at: OffsetDateTime,
}

fn validate_account_name(name: &str) -> ApiResult<()> {
    let ok = (3..=20).contains(&name.chars().count()) && name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_');
    if ok {
        Ok(())
    } else {
        Err(ApiError::bad_request("invalid_name", "Nome de conta: 3 a 20 letras, números ou _."))
    }
}

fn validate_password(password: &str) -> ApiResult<()> {
    if (8..=128).contains(&password.chars().count()) {
        Ok(())
    } else {
        Err(ApiError::bad_request("invalid_password", "Senha: 8 a 128 caracteres."))
    }
}

async fn open_session(state: &AppState, account: Account) -> ApiResult<SessionResponse> {
    let token = auth::new_token();
    let expires_at = OffsetDateTime::now_utc() + Duration::days(SESSION_DAYS);
    sqlx::query("insert into sessions (token_hash, account_id, expires_at) values ($1, $2, $3)")
        .bind(auth::token_hash(&token))
        .bind(account.id)
        .bind(expires_at)
        .execute(&state.db)
        .await?;
    Ok(SessionResponse { token, expires_at, account })
}

pub async fn register(State(state): State<AppState>, Json(req): Json<RegisterRequest>) -> ApiResult<(StatusCode, Json<SessionResponse>)> {
    let name = req.name.trim().to_string();
    validate_account_name(&name)?;
    validate_password(&req.password)?;
    let invite = auth::normalize_invite(&req.invite);
    let password_hash = auth::hash_password(req.password).await?;

    let mut tx = state.db.begin().await?;
    let valid: Option<(String,)> = sqlx::query_as(
        "select code from invites where code = $1 and used_at is null and expires_at > now() for update",
    )
    .bind(&invite)
    .fetch_optional(&mut *tx)
    .await?;
    if valid.is_none() {
        return Err(ApiError::bad_request("invalid_invite", "Convite inválido, expirado ou já usado."));
    }
    let id = Uuid::new_v4();
    let inserted = sqlx::query("insert into accounts (id, name, password_hash) values ($1, $2, $3)")
        .bind(id)
        .bind(&name)
        .bind(&password_hash)
        .execute(&mut *tx)
        .await;
    match inserted {
        Err(e) if is_unique_violation(&e) => return Err(ApiError::conflict("name_taken", "Esse nome de conta já existe.")),
        other => other?,
    };
    sqlx::query("update invites set used_by = $1, used_at = now() where code = $2")
        .bind(id)
        .bind(&invite)
        .execute(&mut *tx)
        .await?;
    tx.commit().await?;
    tracing::info!(account = %name, "conta criada");

    let session = open_session(&state, Account { id, name, is_admin: false }).await?;
    Ok((StatusCode::CREATED, Json(session)))
}

pub async fn login(State(state): State<AppState>, Json(req): Json<LoginRequest>) -> ApiResult<Json<SessionResponse>> {
    let row: Option<(Uuid, String, String, bool)> =
        sqlx::query_as("select id, name, password_hash, is_admin from accounts where lower(name) = lower($1)")
            .bind(req.name.trim())
            .fetch_optional(&state.db)
            .await?;
    let hash = row.as_ref().map(|r| r.2.clone()).unwrap_or_else(|| auth::dummy_hash().to_string());
    let ok = auth::verify_password(req.password, hash).await;
    let Some((id, name, _, is_admin)) = row.filter(|_| ok) else {
        return Err(ApiError::new(StatusCode::UNAUTHORIZED, "bad_credentials", "Conta ou senha incorretas."));
    };
    Ok(Json(open_session(&state, Account { id, name, is_admin }).await?))
}

pub async fn logout(State(state): State<AppState>, session: Session) -> ApiResult<StatusCode> {
    sqlx::query("delete from sessions where token_hash = $1")
        .bind(&session.token_hash)
        .execute(&state.db)
        .await?;
    Ok(StatusCode::NO_CONTENT)
}

pub async fn me(State(state): State<AppState>, session: Session) -> ApiResult<Json<MeResponse>> {
    let characters = list_for(&state, session.account_id).await?;
    Ok(Json(MeResponse {
        account: Account { id: session.account_id, name: session.name, is_admin: session.is_admin },
        characters,
    }))
}

/// Admin gera convite pela API; o primeiro admin sai do `nw-core invite` na linha de comando.
pub async fn create_invite(State(state): State<AppState>, session: Session) -> ApiResult<(StatusCode, Json<InviteResponse>)> {
    if !session.is_admin {
        return Err(ApiError::forbidden());
    }
    let invite = insert_invite(&state.db, Some(session.account_id), INVITE_DAYS).await?;
    Ok((StatusCode::CREATED, Json(invite)))
}

pub async fn insert_invite(db: &sqlx::PgPool, created_by: Option<Uuid>, days: i64) -> Result<InviteResponse, sqlx::Error> {
    let code = auth::new_invite_code();
    let expires_at = OffsetDateTime::now_utc() + Duration::days(days);
    sqlx::query("insert into invites (code, created_by, expires_at) values ($1, $2, $3)")
        .bind(&code)
        .bind(created_by)
        .bind(expires_at)
        .execute(db)
        .await?;
    Ok(InviteResponse { code, expires_at })
}
