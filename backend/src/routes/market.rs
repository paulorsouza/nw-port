//! Posto de trocas. Ordem de venda guarda o item; ordem de compra guarda as moedas. O que se compra cai no armazém
//! da cidade da ordem; o que o dono da ordem de venda recebe vira crédito de moedas (`coin_credits`), resgatado pelo
//! servidor de jogo. Taxas e duração vêm do servidor de jogo (sem fonte nos dados; docs/sistemas.md).

use axum::extract::{Path, Query, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use time::{Duration, OffsetDateTime};
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{ApiError, ApiResult};
use crate::routes::characters::{empty_props, is_bound, validate_props};
use crate::routes::serials;
use crate::routes::journal::{self, Ctx};
use crate::routes::storage::{check_room, deliver, is_territory, Snapshot, SHARED};
use crate::AppState;

const MAX_HOURS: i64 = 24 * 30;
/// Ordens abertas por personagem, compra e venda somadas (dono, 2026-10-14; o jogo usa 100).
const MAX_OPEN_ORDERS: i64 = 50;

#[derive(Serialize, Debug, sqlx::FromRow)]
pub struct Order {
    pub id: Uuid,
    pub territory: String,
    pub kind: String,
    pub character_id: Uuid,
    pub item_id: String,
    pub quantity: i32,
    pub gear_score: i32,
    pub rarity: i16,
    pub props: Value,
    pub unit_price: i64,
    pub fee: i64,
    pub status: String,
    /// Peso unitário em kg (do catálogo do servidor de jogo).
    pub weight: f32,
    #[serde(with = "time::serde::rfc3339")]
    pub created_at: OffsetDateTime,
    #[serde(with = "time::serde::rfc3339")]
    pub expires_at: OffsetDateTime,
    /// Nome do personagem dono da ordem (só na lista do livro).
    #[sqlx(default)]
    pub seller: String,
}

/// Colunas de `Order`, em macro para entrar em `concat!` (o sqlx só aceita SQL estático).
macro_rules! order_columns {
    () => {
        "id, territory, kind, character_id, item_id, quantity, gear_score, rarity, props, unit_price, fee, status, weight, created_at, expires_at"
    };
}

#[derive(Deserialize, Debug)]
pub struct ListQuery {
    pub item_id: Option<String>,
    pub kind: Option<String>,
    pub character_id: Option<Uuid>,
}

#[derive(Deserialize, Debug)]
pub struct CreateOrder {
    pub character_id: Uuid,
    pub kind: String,
    pub item_id: String,
    pub quantity: i32,
    #[serde(default)]
    pub gear_score: i32,
    #[serde(default)]
    pub rarity: i16,
    #[serde(default = "empty_props")]
    pub props: Value,
    pub unit_price: i64,
    #[serde(default)]
    pub fee: i64,
    pub duration_hours: i64,
    /// Peso unitário em kg; a ordem de compra só passa se couber no armazém (409 `storage_full`).
    #[serde(default)]
    pub weight: f32,
    /// Bolsa e moedas do dono depois de tirar o item (venda) ou as moedas (compra) e a taxa.
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

#[derive(Deserialize, Debug)]
pub struct FillOrder {
    pub character_id: Uuid,
    pub quantity: i32,
    /// Ordem de compra: o item que o vendedor entrega (padrão: o da ordem).
    pub gear_score: Option<i32>,
    pub rarity: Option<i16>,
    pub props: Option<Value>,
    /// Bolsa e moedas de quem fecha a ordem, já com o pagamento (venda) ou o item tirado e as moedas (compra).
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

#[derive(Deserialize, Debug)]
pub struct CancelOrder {
    pub character_id: Uuid,
}

#[derive(Serialize, Debug)]
pub struct Claimed {
    pub amount: i64,
}

/// Créditos pendentes até `until` (o resgate leva esse instante e soma exatamente esse valor).
#[derive(Serialize, Debug)]
pub struct Pending {
    pub amount: i64,
    #[serde(with = "time::serde::rfc3339::option")]
    pub until: Option<OffsetDateTime>,
}

#[derive(Deserialize, Debug)]
pub struct ClaimRequest {
    #[serde(flatten)]
    pub write: journal::WriteSeq,
    /// Só os créditos criados até este instante (o da consulta); ausente = todos.
    #[serde(default, with = "time::serde::rfc3339::option")]
    pub until: Option<OffsetDateTime>,
}

pub(crate) async fn credit(tx: &mut sqlx::PgConnection, character_id: Uuid, amount: i64, reason: &str, order: Option<Uuid>) -> ApiResult<()> {
    if amount <= 0 {
        return Ok(());
    }
    sqlx::query("insert into coin_credits (id, character_id, amount, reason, order_id) values ($1, $2, $3, $4, $5)")
        .bind(Uuid::new_v4())
        .bind(character_id)
        .bind(amount)
        .bind(reason)
        .bind(order)
        .execute(&mut *tx)
        .await?;
    journal::log(tx, &Ctx::core(character_id, reason), "credit", None, amount, 0, 0, &serde_json::json!({})).await?;
    Ok(())
}

/// Fecha a ordem sem negociar o resto: item de volta ao armazém (venda) ou moedas de volta em crédito (compra).
async fn close(tx: &mut sqlx::PgConnection, o: &Order, status: &str) -> ApiResult<()> {
    if o.quantity > 0 {
        if o.kind == "sell" {
            let reason = format!("market_{status}");
            journal::log(tx, &Ctx::core(o.character_id, &reason), &format!("market:{}", o.id), Some(&o.item_id), -(o.quantity as i64), o.gear_score, o.rarity, &o.props).await?;
            deliver(tx, o.character_id, SHARED, &o.item_id, o.quantity, o.gear_score, o.rarity, &o.props, o.weight, &reason).await?;
        } else {
            credit(tx, o.character_id, o.unit_price * o.quantity as i64, "market_refund", Some(o.id)).await?;
        }
    }
    sqlx::query("update market_orders set status = $2, closed_at = now() where id = $1")
        .bind(o.id)
        .bind(status)
        .execute(&mut *tx)
        .await?;
    if o.kind == "sell" && o.quantity > 0 {
        serials::verify(tx, o.character_id, SHARED, &serials::serials([&o.props])).await?;
    }
    Ok(())
}

/// Ordens vencidas de um território (ou todas) viram 'expired'.
async fn expire(tx: &mut sqlx::PgConnection, territory: Option<&str>) -> ApiResult<()> {
    let due: Vec<Order> = sqlx::query_as(concat!("select ", order_columns!(), " from market_orders
          where status = 'open' and expires_at <= now() and ($1::text is null or territory = $1)
          for update skip locked"))
    .bind(territory)
    .fetch_all(&mut *tx)
    .await?;
    for o in &due {
        close(tx, o, "expired").await?;
    }
    Ok(())
}

pub async fn list(
    State(state): State<AppState>,
    _server: GameServer,
    Path(territory): Path<String>,
    Query(q): Query<ListQuery>,
) -> ApiResult<Json<Vec<Order>>> {
    if !is_territory(&territory) {
        return Err(ApiError::bad_request("invalid_territory", "Território inválido."));
    }
    let mut tx = state.db.begin().await?;
    expire(&mut tx, Some(&territory)).await?;
    let orders: Vec<Order> = sqlx::query_as(concat!("select ", order_columns!(), ",
            (select name from characters c where c.id = market_orders.character_id) as seller from market_orders
          where territory = $1 and status = 'open'
            and ($2::text is null or item_id = $2) and ($3::text is null or kind = $3)
            and ($4::uuid is null or character_id = $4)
          order by item_id, kind, unit_price, created_at"))
    .bind(&territory)
    .bind(&q.item_id)
    .bind(&q.kind)
    .bind(q.character_id)
    .fetch_all(&mut *tx)
    .await?;
    tx.commit().await?;
    Ok(Json(orders))
}

pub async fn create(
    State(state): State<AppState>,
    _server: GameServer,
    Path(territory): Path<String>,
    Json(c): Json<CreateOrder>,
) -> ApiResult<(StatusCode, Json<Order>)> {
    if !is_territory(&territory) {
        return Err(ApiError::bad_request("invalid_territory", "Território inválido."));
    }
    if c.kind != "sell" && c.kind != "buy" {
        return Err(ApiError::bad_request("invalid_order", "Tipo de ordem inválido."));
    }
    if c.item_id.is_empty() || c.item_id.len() > 80 || c.quantity <= 0 || c.unit_price <= 0 || c.fee < 0
        || !c.weight.is_finite() || c.weight < 0.0
    {
        return Err(ApiError::bad_request("invalid_order", "Item, quantidade, preço ou taxa inválidos."));
    }
    validate_props(&c.props, &c.item_id).map_err(|e| ApiError::bad_request("invalid_order", e))?;
    if is_bound(&c.props) {
        return Err(ApiError::bad_request("item_bound", "Item vinculado não vai ao posto de trocas."));
    }
    if c.unit_price > crate::routes::city::RULES.preco_item_max {
        return Err(ApiError::bad_request("price_too_high", "Preço acima do máximo por item."));
    }
    if !(1..=MAX_HOURS).contains(&c.duration_hours) {
        return Err(ApiError::bad_request("invalid_order", "Duração inválida."));
    }
    c.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    let (open,): (i64,) = sqlx::query_as("select count(*) from market_orders where character_id = $1 and status = 'open'")
        .bind(c.character_id)
        .fetch_one(&mut *tx)
        .await?;
    if open >= MAX_OPEN_ORDERS {
        return Err(ApiError::conflict("too_many_orders", "Limite de ordens abertas."));
    }
    if c.kind == "buy" {
        // Venda não tem limite; compra soma armazém + compras abertas + esta ordem contra a capacidade.
        check_room(&mut tx, c.character_id, c.quantity as f64 * c.weight as f64).await?;
    }
    let ctx = c.snapshot.apply(&mut tx, c.character_id).await?;
    let order: Order = sqlx::query_as(concat!("insert into market_orders (id, territory, kind, character_id, item_id, quantity, gear_score, rarity, props, unit_price, fee, expires_at, weight)
         values ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) returning ", order_columns!(), ""))
    .bind(Uuid::new_v4())
    .bind(&territory)
    .bind(&c.kind)
    .bind(c.character_id)
    .bind(&c.item_id)
    .bind(c.quantity)
    .bind(c.gear_score)
    .bind(c.rarity)
    .bind(&c.props)
    .bind(c.unit_price)
    .bind(c.fee)
    .bind(OffsetDateTime::now_utc() + Duration::hours(c.duration_hours))
    .bind(c.weight)
    .fetch_one(&mut *tx)
    .await?;
    if order.kind == "sell" {
        journal::log(&mut tx, &ctx, &format!("market:{}", order.id), Some(&order.item_id), order.quantity as i64, order.gear_score, order.rarity, &order.props).await?;
    }
    serials::verify(&mut tx, c.character_id, "bag", &c.snapshot.serials()).await?;
    serials::verify(&mut tx, c.character_id, "market", &serials::serials([&order.props])).await?;
    tx.commit().await?;
    Ok((StatusCode::CREATED, Json(order)))
}

async fn lock_order(tx: &mut sqlx::PgConnection, id: Uuid) -> ApiResult<Order> {
    let o: Option<Order> = sqlx::query_as(concat!("select ", order_columns!(), " from market_orders where id = $1 for update"))
        .bind(id)
        .fetch_optional(&mut *tx)
        .await?;
    o.ok_or_else(|| ApiError::not_found("Ordem"))
}

pub async fn fill(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(f): Json<FillOrder>,
) -> ApiResult<Json<Order>> {
    f.snapshot.validate()?;
    if let Some(p) = &f.props {
        validate_props(p, "ordem").map_err(|e| ApiError::bad_request("invalid_order", e))?;
        if is_bound(p) {
            return Err(ApiError::bad_request("item_bound", "Item vinculado não vai ao posto de trocas."));
        }
    }
    let mut tx = state.db.begin().await?;
    let o = lock_order(&mut tx, id).await?;
    if o.status == "open" && o.expires_at <= OffsetDateTime::now_utc() {
        close(&mut tx, &o, "expired").await?;
        tx.commit().await?;
        return Err(ApiError::conflict("order_closed", "A ordem venceu."));
    }
    if o.status != "open" {
        return Err(ApiError::conflict("order_closed", "A ordem não está mais aberta."));
    }
    if o.character_id == f.character_id {
        return Err(ApiError::bad_request("own_order", "Não dá para negociar com a própria ordem."));
    }
    if f.quantity <= 0 || f.quantity > o.quantity {
        return Err(ApiError::conflict("order_quantity", "Quantidade maior que a disponível."));
    }
    if o.kind == "sell" {
        // Comprar de uma ordem de venda também é compra: precisa caber no armazém de quem compra.
        check_room(&mut tx, f.character_id, f.quantity as f64 * o.weight as f64).await?;
    }
    f.snapshot.apply(&mut tx, f.character_id).await?;
    let place = SHARED;
    let total = o.unit_price * f.quantity as i64;
    if o.kind == "sell" {
        journal::log(&mut tx, &Ctx::core(o.character_id, "market_sold"), &format!("market:{}", o.id), Some(&o.item_id), -(f.quantity as i64), o.gear_score, o.rarity, &o.props).await?;
        deliver(&mut tx, f.character_id, place, &o.item_id, f.quantity, o.gear_score, o.rarity, &o.props, o.weight, "market_bought").await?;
        credit(&mut tx, o.character_id, total, "market_sale", Some(o.id)).await?;
    } else {
        let gs = f.gear_score.unwrap_or(o.gear_score);
        let rarity = f.rarity.unwrap_or(o.rarity);
        let props = f.props.as_ref().unwrap_or(&o.props);
        deliver(&mut tx, o.character_id, place, &o.item_id, f.quantity, gs, rarity, props, o.weight, "market_buy_order").await?;
    }
    let left = o.quantity - f.quantity;
    let updated: Order = sqlx::query_as(concat!("update market_orders set quantity = $2,
                status = case when $2 = 0 then 'filled' else status end,
                closed_at = case when $2 = 0 then now() else closed_at end
          where id = $1 returning ", order_columns!(), ""))
    .bind(o.id)
    .bind(left)
    .fetch_one(&mut *tx)
    .await?;
    serials::verify(&mut tx, f.character_id, "bag", &f.snapshot.serials()).await?;
    let (owner, delivered) = if o.kind == "sell" { (f.character_id, &o.props) } else { (o.character_id, f.props.as_ref().unwrap_or(&o.props)) };
    serials::verify(&mut tx, owner, place, &serials::serials([delivered])).await?;
    tx.commit().await?;
    Ok(Json(updated))
}

pub async fn cancel(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(c): Json<CancelOrder>,
) -> ApiResult<StatusCode> {
    let mut tx = state.db.begin().await?;
    let o = lock_order(&mut tx, id).await?;
    if o.character_id != c.character_id {
        return Err(ApiError::forbidden());
    }
    if o.status != "open" {
        return Err(ApiError::conflict("order_closed", "A ordem não está mais aberta."));
    }
    close(&mut tx, &o, "cancelled").await?;
    tx.commit().await?;
    Ok(StatusCode::NO_CONTENT)
}

/// Moedas a receber (vendas e devoluções). O servidor de jogo soma no personagem e salva logo em seguida.
/// Resgata os créditos somando nas moedas do personagem, numa transação com sessão e número (o servidor de jogo soma o
/// `amount` devolvido nas moedas que tem em memória).
pub async fn claim_credits(
    State(state): State<AppState>,
    _server: GameServer,
    Path(id): Path<Uuid>,
    Json(c): Json<ClaimRequest>,
) -> ApiResult<Json<Claimed>> {
    let mut tx = state.db.begin().await?;
    expire(&mut tx, None).await?;
    let ctx = journal::claim(&mut tx, id, &c.write).await?;
    let (amount,): (Option<i64>,) = sqlx::query_as(
        "with c as (update coin_credits set claimed_at = now()
                     where character_id = $1 and claimed_at is null and ($2::timestamptz is null or created_at <= $2)
                     returning amount)
         select sum(amount)::bigint from c",
    )
    .bind(id)
    .bind(c.until)
    .fetch_one(&mut *tx)
    .await?;
    let amount = amount.unwrap_or(0);
    if amount > 0 {
        sqlx::query("update characters set coins = coins + $2 where id = $1").bind(id).bind(amount).execute(&mut *tx).await?;
        journal::log(&mut tx, &ctx, "credit", None, -amount, 0, 0, &serde_json::json!({})).await?;
        journal::log(&mut tx, &ctx, "coins", None, amount, 0, 0, &serde_json::json!({})).await?;
    }
    tx.commit().await?;
    Ok(Json(Claimed { amount }))
}

/// `GET /internal/characters/{id}/credits`: créditos de venda pendentes (vence ordens antes de somar).
pub async fn pending_credits(State(state): State<AppState>, _server: GameServer, Path(id): Path<Uuid>) -> ApiResult<Json<Pending>> {
    let mut tx = state.db.begin().await?;
    expire(&mut tx, None).await?;
    let (amount, until): (Option<i64>, Option<OffsetDateTime>) = sqlx::query_as(
        "select sum(amount)::bigint, max(created_at) from coin_credits where character_id = $1 and claimed_at is null",
    )
    .bind(id)
    .fetch_one(&mut *tx)
    .await?;
    tx.commit().await?;
    Ok(Json(Pending { amount: amount.unwrap_or(0), until }))
}
