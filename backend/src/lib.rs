//! nw-core: backend do servidor do clã. Contas por convite, sessões, personagens e o passe de entrada que o
//! servidor de jogo (Unreal dedicado) resgata para carregar o personagem.

pub mod auth;
pub mod error;
pub mod routes;

use std::sync::Arc;

use axum::Router;
use sqlx::PgPool;

pub static MIGRATOR: sqlx::migrate::Migrator = sqlx::migrate!();

#[derive(Clone)]
pub struct AppState {
    pub db: PgPool,
    /// Segredo compartilhado com os servidores de jogo (cabeçalho `X-NW-Server-Secret`).
    pub server_secret: Arc<str>,
}

pub fn app(state: AppState) -> Router {
    routes::router()
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .with_state(state)
}
