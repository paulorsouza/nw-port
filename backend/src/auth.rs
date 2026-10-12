//! Senhas (Argon2id), tokens opacos (guardados como SHA-256) e os extratores de sessão do cliente e do
//! servidor de jogo.

use argon2::password_hash::phc::PasswordHash;
use argon2::password_hash::{PasswordHasher, PasswordVerifier};
use argon2::Argon2;
use axum::extract::FromRequestParts;
use axum::http::request::Parts;
use base64::engine::general_purpose::URL_SAFE_NO_PAD;
use base64::Engine;
use sha2::{Digest, Sha256};
use uuid::Uuid;

use crate::error::ApiError;
use crate::AppState;

pub async fn hash_password(password: String) -> anyhow::Result<String> {
    tokio::task::spawn_blocking(move || {
        Argon2::default()
            .hash_password(password.as_bytes())
            .map(|h| h.to_string())
            .map_err(|e| anyhow::anyhow!("hash de senha: {e}"))
    })
    .await?
}

pub async fn verify_password(password: String, hash: String) -> bool {
    tokio::task::spawn_blocking(move || {
        PasswordHash::new(&hash)
            .map(|parsed| Argon2::default().verify_password(password.as_bytes(), &parsed).is_ok())
            .unwrap_or(false)
    })
    .await
    .unwrap_or(false)
}

/// Hash usado quando a conta não existe, para o login levar o mesmo tempo.
pub fn dummy_hash() -> &'static str {
    static DUMMY: std::sync::OnceLock<String> = std::sync::OnceLock::new();
    DUMMY.get_or_init(|| Argon2::default().hash_password(b"nw-core-dummy").unwrap().to_string())
}

/// Token aleatório de 256 bits em base64url.
pub fn new_token() -> String {
    let mut bytes = [0u8; 32];
    rand::fill(&mut bytes);
    URL_SAFE_NO_PAD.encode(bytes)
}

pub fn token_hash(token: &str) -> Vec<u8> {
    Sha256::digest(token.as_bytes()).to_vec()
}

/// Código de convite: 16 caracteres sem ambíguos (0/O, 1/I/L), em grupos de 4.
pub fn new_invite_code() -> String {
    const ALPHABET: &[u8] = b"23456789ABCDEFGHJKMNPQRSTUVWXYZ";
    let mut bytes = [0u8; 16];
    rand::fill(&mut bytes);
    let chars: Vec<char> = bytes.iter().map(|b| ALPHABET[*b as usize % ALPHABET.len()] as char).collect();
    chars.chunks(4).map(|c| c.iter().collect::<String>()).collect::<Vec<_>>().join("-")
}

pub fn normalize_invite(code: &str) -> String {
    let clean: String = code.chars().filter(|c| c.is_ascii_alphanumeric()).map(|c| c.to_ascii_uppercase()).collect();
    clean.as_bytes().chunks(4).map(|c| String::from_utf8_lossy(c).into_owned()).collect::<Vec<_>>().join("-")
}

fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    a.len() == b.len() && a.iter().zip(b).fold(0u8, |acc, (x, y)| acc | (x ^ y)) == 0
}

/// Jogador autenticado (cabeçalho `Authorization: Bearer <token>`).
#[derive(Debug, Clone)]
pub struct Session {
    pub account_id: Uuid,
    pub name: String,
    pub is_admin: bool,
    pub token_hash: Vec<u8>,
}

impl FromRequestParts<AppState> for Session {
    type Rejection = ApiError;

    async fn from_request_parts(parts: &mut Parts, state: &AppState) -> Result<Self, Self::Rejection> {
        let token = parts
            .headers
            .get(axum::http::header::AUTHORIZATION)
            .and_then(|v| v.to_str().ok())
            .and_then(|v| v.strip_prefix("Bearer "))
            .ok_or_else(ApiError::unauthorized)?;
        let hash = token_hash(token.trim());
        let row: Option<(Uuid, String, bool)> = sqlx::query_as(
            "select a.id, a.name, a.is_admin
               from sessions s join accounts a on a.id = s.account_id
              where s.token_hash = $1 and s.expires_at > now()",
        )
        .bind(&hash)
        .fetch_optional(&state.db)
        .await?;
        let (account_id, name, is_admin) = row.ok_or_else(ApiError::unauthorized)?;
        Ok(Session { account_id, name, is_admin, token_hash: hash })
    }
}

/// Chamada de um servidor de jogo (cabeçalho `X-NW-Server-Secret`).
pub struct GameServer;

impl FromRequestParts<AppState> for GameServer {
    type Rejection = ApiError;

    async fn from_request_parts(parts: &mut Parts, state: &AppState) -> Result<Self, Self::Rejection> {
        let given = parts.headers.get("x-nw-server-secret").map(|v| v.as_bytes()).unwrap_or_default();
        if state.server_secret.is_empty() || !constant_time_eq(given, state.server_secret.as_bytes()) {
            return Err(ApiError::unauthorized());
        }
        Ok(GameServer)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn invite_code_format() {
        let code = new_invite_code();
        assert_eq!(code.len(), 19);
        assert_eq!(normalize_invite(&code.to_lowercase().replace('-', " ")), code);
    }

    #[tokio::test]
    async fn password_roundtrip() {
        let hash = hash_password("segredo-forte".into()).await.unwrap();
        assert!(verify_password("segredo-forte".into(), hash.clone()).await);
        assert!(!verify_password("errada".into(), hash).await);
    }
}
