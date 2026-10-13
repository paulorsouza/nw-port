//! Número de série dos itens assinados (docs/itens.md, Assinatura). O servidor de jogo pede o número ao fabricar; o
//! item leva o número em `props.serial`. Toda gravação que leva item com número chama `verify` no fim da transação:
//! o número tem que existir no registro e aparecer uma vez só (bolsa + armazém + ordens abertas), senão a gravação
//! inteira é recusada. A linha do registro fica travada até o fim, então duas gravações com o mesmo item não passam juntas.

use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;
use serde::Serialize;
use serde_json::Value;
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{ApiError, ApiResult};
use crate::AppState;

#[derive(Serialize, Debug)]
pub struct Issued {
    pub serial: String,
}

/// Números de série nos `props` (repetidos ficam: a contagem pega a repetição).
pub(crate) fn serials<'a>(props: impl IntoIterator<Item = &'a Value>) -> Vec<String> {
    props
        .into_iter()
        .filter_map(|p| p.get("serial").and_then(Value::as_str).map(str::to_owned))
        .collect()
}

/// Emite o próximo número do artesão (o personagem que fabricou).
pub async fn issue(State(state): State<AppState>, _server: GameServer, Path(id): Path<Uuid>) -> ApiResult<(StatusCode, Json<Issued>)> {
    let mut tx = state.db.begin().await?;
    let seq: Option<(i64,)> = sqlx::query_as(
        "insert into crafter_sequences (character_id, last) select id, 1 from characters where id = $1 and deleted_at is null
         on conflict (character_id) do update set last = crafter_sequences.last + 1 returning last",
    )
    .bind(id)
    .fetch_optional(&mut *tx)
    .await?;
    let (seq,) = seq.ok_or_else(|| ApiError::not_found("Personagem"))?;
    let serial = format!("{}-{}", id.simple(), seq);
    sqlx::query("insert into item_serials (serial, crafter_id, character_id, location) values ($1, $2, $2, 'bag')")
        .bind(&serial)
        .bind(id)
        .execute(&mut *tx)
        .await?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(Issued { serial })))
}

/// No fim da transação de quem grava: números conhecidos, um só de cada e dono/local atualizados.
pub(crate) async fn verify(tx: &mut sqlx::PgConnection, character_id: Uuid, location: &str, serials: &[String]) -> ApiResult<()> {
    if serials.is_empty() {
        return Ok(());
    }
    let mut unique = serials.to_vec();
    unique.sort();
    unique.dedup();
    if unique.len() != serials.len() {
        return Err(ApiError::conflict("item_duplicate", "Item assinado repetido na mesma gravação."));
    }
    let known: Vec<(String,)> = sqlx::query_as("select serial from item_serials where serial = any($1) order by serial for update")
        .bind(&unique)
        .fetch_all(&mut *tx)
        .await?;
    if known.len() != unique.len() {
        return Err(ApiError::bad_request("unknown_serial", "Número de série desconhecido."));
    }
    let dup: Option<(String,)> = sqlx::query_as(
        "select s from (
            select props ->> 'serial' as s from items where props ? 'serial'
            union all select props ->> 'serial' from storage_items where props ? 'serial'
            union all select props ->> 'serial' from market_orders where status = 'open' and props ? 'serial'
         ) t where s = any($1) group by s having count(*) > 1 limit 1",
    )
    .bind(&unique)
    .fetch_optional(&mut *tx)
    .await?;
    if let Some((s,)) = dup {
        tracing::warn!(serial = %s, character = %character_id, "item assinado duplicado: gravação recusada");
        return Err(ApiError::conflict("item_duplicate", "Item assinado já existe em outro lugar."));
    }
    sqlx::query("update item_serials set character_id = $2, location = $3, updated_at = now() where serial = any($1)")
        .bind(&unique)
        .bind(character_id)
        .bind(location)
        .execute(&mut *tx)
        .await?;
    Ok(())
}
