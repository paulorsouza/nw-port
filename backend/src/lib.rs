//! nw-core: backend for the clan server. Invite-only accounts, sessions, characters and the entry ticket that the
//! game server (Unreal dedicated) redeems to load the character.

pub mod auth;
pub mod error;
pub mod maps;
pub mod routes;

use std::sync::Arc;

use axum::Router;
use sqlx::PgPool;

pub static MIGRATOR: sqlx::migrate::Migrator = sqlx::migrate!();

#[derive(Clone)]
pub struct AppState {
    pub db: PgPool,
    /// Secret shared with the game servers (`X-NW-Server-Secret` header).
    pub server_secret: Arc<str>,
    /// Game server of each map (`NW_MAP_SERVERS`); empty = one server, the client's own address.
    pub map_servers: Arc<maps::MapServers>,
}

pub fn app(state: AppState) -> Router {
    routes::router()
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .with_state(state)
}
