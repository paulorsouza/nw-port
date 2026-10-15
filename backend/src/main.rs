use std::sync::Arc;

use anyhow::Context;
use clap::{Parser, Subcommand};
use sqlx::postgres::PgPoolOptions;
use tracing_subscriber::EnvFilter;

use nw_core::{app, AppState, MIGRATOR};

#[derive(Parser)]
#[command(name = "nw-core", about = "Backend for the clan server")]
struct Cli {
    #[arg(long, env = "DATABASE_URL")]
    database_url: String,
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Starts the API (applies the migrations first).
    Serve {
        #[arg(long, env = "NW_BIND", default_value = "0.0.0.0:8090")]
        bind: String,
        /// Game server secret.
        #[arg(long, env = "NW_SERVER_SECRET")]
        server_secret: String,
        /// Game server of each map: "L_SouthWindsward=host:7777,L_QuedaEterna=host:7778,L_Arena=host:7779".
        #[arg(long, env = "NW_MAP_SERVERS", default_value = "")]
        map_servers: String,
    },
    /// Only applies the migrations.
    Migrate,
    /// Generates account invites.
    Invite {
        #[arg(long, default_value_t = 1)]
        count: u32,
        #[arg(long, default_value_t = 7)]
        days: i64,
    },
    /// Grants or revokes admin on an account.
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
        .context("connect to Postgres")?;

    match cli.command {
        Command::Serve { bind, server_secret, map_servers } => {
            let map_servers = nw_core::maps::MapServers::parse(&map_servers)?;
            anyhow::ensure!(server_secret.len() >= 16, "NW_SERVER_SECRET needs 16+ characters");
            MIGRATOR.run(&db).await?;
            let listener = tokio::net::TcpListener::bind(&bind).await.with_context(|| format!("bind {bind}"))?;
            tracing::info!(%bind, "nw-core up");
            // Cities: attacks and week rollover kept up to date even when nobody touches them.
            let cities_db = db.clone();
            tokio::spawn(async move {
                let mut tick = tokio::time::interval(std::time::Duration::from_secs(60));
                loop {
                    tick.tick().await;
                    if let Err(e) = nw_core::routes::advance_cities(&cities_db, time::OffsetDateTime::now_utc()).await {
                        tracing::warn!(?e, "cities: rollover/attacks");
                    }
                }
            });
            let state = AppState { db, server_secret: Arc::from(server_secret), map_servers: Arc::new(map_servers) };
            axum::serve(listener, app(state)).with_graceful_shutdown(shutdown()).await?;
        }
        Command::Migrate => {
            MIGRATOR.run(&db).await?;
            println!("migrations applied");
        }
        Command::Invite { count, days } => {
            MIGRATOR.run(&db).await?;
            for _ in 0..count {
                let invite = nw_core::routes::insert_invite(&db, None, days).await?;
                println!("{}  (valid until {})", invite.code, invite.expires_at.date());
            }
        }
        Command::Admin { name, revoke } => {
            let done = sqlx::query("update accounts set is_admin = $2 where lower(name) = lower($1)")
                .bind(&name)
                .bind(!revoke)
                .execute(&db)
                .await?;
            anyhow::ensure!(done.rows_affected() == 1, "account {name} does not exist");
            println!("{name}: admin = {}", !revoke);
        }
    }
    Ok(())
}

async fn shutdown() {
    let _ = tokio::signal::ctrl_c().await;
}
