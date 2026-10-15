mod account;
mod characters;
mod city;
mod ground;
mod houses;
mod internal;
mod journal;
mod market;
mod npc_rewards;
mod serials;
mod storage;

use axum::routing::{delete, get, post, put};
use axum::Router;

use crate::AppState;

pub use account::insert_invite;
pub use characters::{Character, CharacterState, ItemInstance, Position};
pub use city::advance_all as advance_cities;

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
        .route("/internal/characters/{id}/credits", get(market::pending_credits))
        .route("/internal/characters/{id}/credits/claim", post(market::claim_credits))
        .route("/internal/characters/{id}/npc-rewards", get(npc_rewards::list).post(npc_rewards::claim))
        .route("/internal/characters/{id}/serials", post(serials::issue))
        .route("/internal/market/{territory}/orders", get(market::list).post(market::create))
        .route("/internal/market/orders/{id}/fill", post(market::fill))
        .route("/internal/market/orders/{id}/cancel", post(market::cancel))
        .route("/internal/item-log", get(journal::list))
        .route("/internal/regions/{region}/city", get(city::get).put(city::manage))
        .route("/internal/regions/{region}/city/log", get(city::log_list))
        .route("/internal/regions/{region}/city/buy", post(city::buy_city))
        .route("/internal/regions/{region}/city/sale", post(city::sale))
        .route("/internal/regions/{region}/city/bid", post(city::bid))
        .route("/internal/regions/{region}/city/sell-raw", post(city::sell_raw))
        .route("/internal/regions/{region}/city/buy-item", post(city::buy_item))
        .route("/internal/regions/{region}/city/projects", post(city::project))
        .route("/internal/cities/advance", post(city::advance))
        .route("/internal/characters/{id}/ground", get(ground::list).post(ground::drop_items))
        .route("/internal/ground/{bag}/take", post(ground::take))
}
