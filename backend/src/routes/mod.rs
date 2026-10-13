mod account;
mod characters;
mod houses;
mod internal;
mod market;
mod npc_rewards;
mod storage;

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
        .route("/internal/characters/{id}/storage/{location}", get(storage::get).put(storage::put))
        .route("/internal/characters/{id}/houses", get(houses::list).post(houses::buy))
        .route("/internal/characters/{id}/credits/claim", post(market::claim_credits))
        .route("/internal/characters/{id}/npc-rewards", get(npc_rewards::list).post(npc_rewards::claim))
        .route("/internal/market/{territory}/orders", get(market::list).post(market::create))
        .route("/internal/market/orders/{id}/fill", post(market::fill))
        .route("/internal/market/orders/{id}/cancel", post(market::cancel))
}
