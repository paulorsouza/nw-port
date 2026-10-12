use std::sync::Arc;

use anyhow::Context;
use clap::{Parser, Subcommand};
use sqlx::postgres::PgPoolOptions;
use tracing_subscriber::EnvFilter;

use nw_core::{app, AppState, MIGRATOR};

#[derive(Parser)]
#[command(name = "nw-core", about = "Backend do servidor do clã")]
struct Cli {
    #[arg(long, env = "DATABASE_URL")]
    database_url: String,
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Sobe a API (aplica as migrações antes).
    Serve {
        #[arg(long, env = "NW_BIND", default_value = "0.0.0.0:8090")]
        bind: String,
        /// Segredo dos servidores de jogo.
        #[arg(long, env = "NW_SERVER_SECRET")]
        server_secret: String,
    },
    /// Só aplica as migrações.
    Migrate,
    /// Gera convites de conta.
    Invite {
        #[arg(long, default_value_t = 1)]
        count: u32,
        #[arg(long, default_value_t = 7)]
        days: i64,
    },
    /// Dá ou tira admin de uma conta.
    Admin {
        name: String,
        #[arg(long)]
        revoke: bool,
    },
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info,nw_core=debug")))
        .with_writer(std::io::stderr)
        .init();
    let cli = Cli::parse();
    let db = PgPoolOptions::new()
        .max_connections(16)
        .connect(&cli.database_url)
        .await
        .context("conectar no Postgres")?;

    match cli.command {
        Command::Serve { bind, server_secret } => {
            anyhow::ensure!(server_secret.len() >= 16, "NW_SERVER_SECRET precisa de 16+ caracteres");
            MIGRATOR.run(&db).await?;
            let listener = tokio::net::TcpListener::bind(&bind).await.with_context(|| format!("bind {bind}"))?;
            tracing::info!(%bind, "nw-core no ar");
            let state = AppState { db, server_secret: Arc::from(server_secret) };
            axum::serve(listener, app(state)).with_graceful_shutdown(shutdown()).await?;
        }
        Command::Migrate => {
            MIGRATOR.run(&db).await?;
            println!("migrações aplicadas");
        }
        Command::Invite { count, days } => {
            MIGRATOR.run(&db).await?;
            for _ in 0..count {
                let invite = nw_core::routes::insert_invite(&db, None, days).await?;
                println!("{}  (vale até {})", invite.code, invite.expires_at.date());
            }
        }
        Command::Admin { name, revoke } => {
            let done = sqlx::query("update accounts set is_admin = $2 where lower(name) = lower($1)")
                .bind(&name)
                .bind(!revoke)
                .execute(&db)
                .await?;
            anyhow::ensure!(done.rows_affected() == 1, "conta {name} não existe");
            println!("{name}: admin = {}", !revoke);
        }
    }
    Ok(())
}

async fn shutdown() {
    let _ = tokio::signal::ctrl_c().await;
}
