mod account;
mod characters;
mod internal;

use axum::routing::{delete, get, post, put};
use axum::Router;

use crate::AppState;

pub use account::insert_invite;
pub use characters::{Character, CharacterState, ItemInstance, Position};

pub fn router() -> Router<AppState> {
    Router::new()
        .route("/health", get(|| async { "ok" }))
        .route("/v1/auth/register", post(account::register))
        .route("/v1/auth/login", post(account::login))
        .route("/v1/auth/logout", post(account::logout))
        .route("/v1/me", get(account::me))
        .route("/v1/invites", post(account::create_invite))
        .route("/v1/characters", get(characters::list).post(characters::create))
        .route("/v1/characters/{id}", delete(characters::remove))
        .route("/v1/characters/{id}/join", post(characters::join))
        .route("/internal/tickets/redeem", post(internal::redeem_ticket))
        .route("/internal/characters/{id}/state", put(internal::save_state))
}
