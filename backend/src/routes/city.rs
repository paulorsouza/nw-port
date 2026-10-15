//! City governance (migration 0013, docs/game/economy.md). One city per region; channels of the same region
//! share everything. The city buys raw materials and sells its list at the governor's price (within the ranges of
//! tools/precos_cidade.json), collects the crafting tax and spends the treasury on stations, buffs and weapons. At the
//! week rollover: station upkeep, goal, tier and governor prize (`tenebroso` = forced sale). Every 2 days
//! after the owner's first week: a simulated attack against the city's weapons.
//!
//! Rules in tools/governanca.json (read here at build time). Coins in cents.

use std::collections::{BTreeMap, HashMap};
use std::sync::LazyLock;

use axum::extract::{Path, Query, State};
use axum::http::StatusCode;
use axum::Json;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use time::{Duration, OffsetDateTime, Time, Weekday};
use uuid::Uuid;

use crate::auth::GameServer;
use crate::error::{ApiError, ApiResult};
use crate::routes::characters::empty_props;
use crate::routes::journal;
use crate::routes::market::credit;
use crate::routes::storage::Snapshot;
use crate::AppState;

// ---------------------------------------------------------------- rules

#[derive(Deserialize)]
pub(crate) struct Rules {
    pub regioes: Vec<RegionRule>,
    pub preco_cidade: i64,
    pub balancete_inicial: i64,
    pub taxa_revenda: f64,
    pub carencia_dias: i64,
    pub imposto: TaxRange,
    pub limite_semanal: Limits,
    /// Station -> upkeep raw material.
    pub mesas: BTreeMap<String, String>,
    pub mesa_escala: TierRule,
    pub mesa_nivel: LevelRule,
    pub armas: WeaponRule,
    pub obra_horas: i64,
    pub conserto_fracao: f64,
    pub manutencao_por_fabricacao_nivel: i64,
    pub buffs: BTreeMap<String, Buff>,
    pub ataque: Attack,
    pub premio: Prize,
    pub leilao: AuctionRule,
    /// Coin cap per character (cents): the excess is not added (maintainer 2026-10-15).
    pub moedas_max: i64,
    /// Maximum unit price on the market (cents).
    pub preco_item_max: i64,
}

#[derive(Deserialize)]
pub(crate) struct AuctionRule {
    pub horas: i64,
    pub incremento_minimo: f64,
}

#[derive(Deserialize)]
pub(crate) struct RegionRule {
    pub id: String,
    pub nome: String,
}

#[derive(Deserialize)]
pub(crate) struct TaxRange {
    pub min: f64,
    pub max: f64,
    pub padrao: f64,
}

#[derive(Deserialize)]
pub(crate) struct Limits {
    pub compra_por_materia_prima: i64,
    pub venda_por_item: i64,
}

/// One upgrade (tier, level or weapon): basket of raw materials from stock and coins from the treasury.
#[derive(Deserialize)]
pub(crate) struct Up {
    #[serde(default)]
    pub para: Option<i32>,
    #[serde(default)]
    pub nivel: Option<i32>,
    pub materiais: BTreeMap<String, i64>,
    pub moedas: i64,
}

#[derive(Deserialize)]
pub(crate) struct TierRule {
    pub inicial: i32,
    pub subir: Vec<Up>,
}

#[derive(Deserialize)]
pub(crate) struct LevelRule {
    pub max: i32,
    pub subir: Vec<Up>,
}

#[derive(Deserialize)]
pub(crate) struct WeaponRule {
    pub tipos: Vec<String>,
    pub inicial: i32,
    pub subir: Vec<Up>,
}

#[derive(Deserialize)]
pub(crate) struct Buff {
    pub custo: i64,
}

#[derive(Deserialize)]
pub(crate) struct Attack {
    pub primeiro_apos_dias: i64,
    pub intervalo_horas: i64,
    pub base: f64,
    pub por_semana: f64,
    pub por_nivel: f64,
    pub variacao: f64,
    pub niveis_perdidos: usize,
}

#[derive(Deserialize)]
pub(crate) struct Prize {
    pub tenebroso: i64,
    pub ruim: i64,
    pub bom: i64,
    pub muito_bom: i64,
    pub excelente: i64,
    pub melhor: i64,
    pub faixas: Tiers,
    pub peso_nivel: i64,
    pub tenebroso_materia_prima: i64,
    pub tenebroso_lista: i64,
}

#[derive(Deserialize)]
pub(crate) struct Tiers {
    pub bom: f64,
    pub muito_bom: f64,
    pub excelente: f64,
}

#[derive(Deserialize, Clone, Copy, Serialize)]
pub(crate) struct PriceRange {
    pub min: i64,
    pub max: i64,
    pub padrao: i64,
    /// Weekly per-character limit for this item only (without it, the rules' `limite_semanal` applies).
    #[serde(default, skip_serializing)]
    pub limite: Option<i64>,
}

#[derive(Deserialize)]
pub(crate) struct Prices {
    pub compra: BTreeMap<String, PriceRange>,
    pub venda: BTreeMap<String, PriceRange>,
}

pub(crate) static RULES: LazyLock<Rules> =
    LazyLock::new(|| serde_json::from_str(include_str!("../../../tools/governanca.json")).expect("tools/governanca.json"));
pub(crate) static PRICES: LazyLock<Prices> =
    LazyLock::new(|| serde_json::from_str(include_str!("../../../tools/precos_cidade.json")).expect("tools/precos_cidade.json"));

/// Maximum station tier (after it, levels).
const TOP_TIER: i32 = 5;
const STATION_COLS: &str = "station, level, reached, crafts, tier, work, work_target, work_ends_at";

// ---------------------------------------------------------------- week

/// Start of the week (Tuesday 08:00 UTC, the same as the NPC rewards).
pub(crate) fn week_start(now: OffsetDateTime) -> OffsetDateTime {
    let t = now - Duration::hours(8);
    let back = (t.weekday().number_days_from_monday() as i64 + 7 - Weekday::Tuesday.number_days_from_monday() as i64) % 7;
    (t.date() - Duration::days(back)).with_time(Time::from_hms(8, 0, 0).unwrap()).assume_utc()
}

pub(crate) fn week_id(now: OffsetDateTime) -> String {
    let d = week_start(now).date();
    format!("{:04}-{:02}-{:02}", d.year(), d.month() as u8, d.day())
}

fn week_begin(id: &str) -> Option<OffsetDateTime> {
    let f = time::macros::format_description!("[year]-[month]-[day]");
    Some(time::Date::parse(id, f).ok()?.with_time(Time::from_hms(8, 0, 0).unwrap()).assume_utc())
}

// ---------------------------------------------------------------- state

#[derive(sqlx::FromRow, Debug, Clone)]
struct CityRow {
    region: String,
    name: String,
    owner_id: Option<Uuid>,
    owner_since: Option<OffsetDateTime>,
    paid: i64,
    treasury: i64,
    tax: f32,
    buy_prices: Value,
    sell_prices: Value,
    next_attack_at: Option<OffsetDateTime>,
    last_attack: Option<Value>,
    week: String,
    auction_start: Option<i64>,
    auction_ends_at: Option<OffsetDateTime>,
    auction_forced: bool,
    auction_bid: Option<i64>,
    auction_bidder: Option<Uuid>,
}

#[derive(sqlx::FromRow, Debug, Clone, Serialize)]
pub struct StationRow {
    pub station: String,
    /// Level after tier 5.
    pub level: i32,
    pub reached: i32,
    pub crafts: i32,
    pub tier: i32,
    /// Work in progress: "tier", "level" or "repair".
    pub work: Option<String>,
    pub work_target: Option<i32>,
    #[serde(with = "time::serde::rfc3339::option")]
    pub work_ends_at: Option<OffsetDateTime>,
}

#[derive(sqlx::FromRow, Debug, Clone, Serialize)]
pub struct WeaponRow {
    pub weapon: String,
    pub tier: i32,
    pub work_target: Option<i32>,
    #[serde(with = "time::serde::rfc3339::option")]
    pub work_ends_at: Option<OffsetDateTime>,
}

async fn stations_of(tx: &mut sqlx::PgConnection, region: &str) -> ApiResult<Vec<StationRow>> {
    Ok(sqlx::query_as(sqlx::AssertSqlSafe(format!("select {STATION_COLS} from city_stations where region = $1 order by station")))
        .bind(region)
        .fetch_all(&mut *tx)
        .await?)
}

async fn weapons_of(tx: &mut sqlx::PgConnection, region: &str) -> ApiResult<Vec<WeaponRow>> {
    Ok(sqlx::query_as("select weapon, tier, work_target, work_ends_at from city_weapons where region = $1 order by weapon")
        .bind(region)
        .fetch_all(&mut *tx)
        .await?)
}

/// Current price of an item: the governor's or the default.
fn price_of(prices: &Value, item: &str, range: &PriceRange) -> i64 {
    prices.get(item).and_then(Value::as_i64).unwrap_or(range.padrao)
}

/// Creates the region's city (rules) if it does not exist yet; returns the locked row.
async fn lock_city(tx: &mut sqlx::PgConnection, region: &str) -> ApiResult<CityRow> {
    let Some(rule) = RULES.regioes.iter().find(|r| r.id == region) else {
        return Err(ApiError::not_found("Região não encontrada."));
    };
    sqlx::query(
        "insert into cities (region, name, treasury, tax, week) values ($1, $2, $3, $4, $5) on conflict (region) do nothing",
    )
    .bind(region)
    .bind(&rule.nome)
    .bind(RULES.balancete_inicial)
    .bind(RULES.imposto.padrao as f32)
    .bind(week_id(OffsetDateTime::now_utc()))
    .execute(&mut *tx)
    .await?;
    for station in RULES.mesas.keys() {
        sqlx::query("insert into city_stations (region, station, tier) values ($1, $2, $3) on conflict do nothing")
            .bind(region)
            .bind(station)
            .bind(RULES.mesa_escala.inicial)
            .execute(&mut *tx)
            .await?;
    }
    for weapon in &RULES.armas.tipos {
        sqlx::query("insert into city_weapons (region, weapon, tier) values ($1, $2, $3) on conflict do nothing")
            .bind(region)
            .bind(weapon)
            .bind(RULES.armas.inicial)
            .execute(&mut *tx)
            .await?;
    }
    let c: CityRow = sqlx::query_as("select * from cities where region = $1 for update").bind(region).fetch_one(&mut *tx).await?;
    sqlx::query("insert into city_weeks (region, week, owner_id) values ($1, $2, $3) on conflict do nothing")
        .bind(region)
        .bind(&c.week)
        .bind(c.owner_id)
        .execute(&mut *tx)
        .await?;
    Ok(c)
}

async fn clog(tx: &mut sqlx::PgConnection, region: &str, kind: &str, who: Option<Uuid>, item: Option<&str>, qty: i64, coins: i64, detail: Value) -> ApiResult<()> {
    sqlx::query("insert into city_log (region, kind, character_id, item_id, quantity, coins, detail) values ($1, $2, $3, $4, $5, $6, $7)")
        .bind(region)
        .bind(kind)
        .bind(who)
        .bind(item)
        .bind(qty)
        .bind(coins)
        .bind(detail)
        .execute(&mut *tx)
        .await?;
    Ok(())
}

async fn set_treasury(tx: &mut sqlx::PgConnection, region: &str, delta: i64) -> ApiResult<()> {
    let r = sqlx::query("update cities set treasury = treasury + $2 where region = $1 and treasury + $2 >= 0")
        .bind(region)
        .bind(delta)
        .execute(&mut *tx)
        .await?;
    if r.rows_affected() == 0 {
        return Err(ApiError::conflict("city_no_funds", "O balancete da cidade não tem moedas para isso."));
    }
    Ok(())
}

async fn week_add(tx: &mut sqlx::PgConnection, region: &str, week: &str, col: &str, delta: i64) -> ApiResult<()> {
    let sql = match col {
        "raw_units" => "update city_weeks set raw_units = raw_units + $3 where region = $1 and week = $2",
        "raw_coins" => "update city_weeks set raw_coins = raw_coins + $3 where region = $1 and week = $2",
        "repair_value" => "update city_weeks set repair_value = repair_value + $3 where region = $1 and week = $2",
        "list_units" => "update city_weeks set list_units = list_units + $3 where region = $1 and week = $2",
        "list_coins" => "update city_weeks set list_coins = list_coins + $3 where region = $1 and week = $2",
        "tax_coins" => "update city_weeks set tax_coins = tax_coins + $3 where region = $1 and week = $2",
        "project_value" => "update city_weeks set project_value = project_value + $3 where region = $1 and week = $2",
        "buffs" => "update city_weeks set buffs = buffs + $3::int where region = $1 and week = $2",
        _ => unreachable!("week column: {col}"),
    };
    sqlx::query(sql).bind(region).bind(week).bind(delta).execute(&mut *tx).await?;
    Ok(())
}

fn require_owner(c: &CityRow, who: Uuid) -> ApiResult<()> {
    if c.owner_id != Some(who) {
        return Err(ApiError::conflict("not_governor", "Só o governador da cidade."));
    }
    Ok(())
}

// ---------------------------------------------------------------- read

#[derive(Serialize)]
pub struct PriceView {
    pub item_id: String,
    pub price: i64,
    #[serde(flatten)]
    pub range: PriceRange,
    /// With `character_id`: how much they already sold (city buys) or bought (list) this week.
    pub used: i64,
    pub limit: i64,
}

#[derive(Serialize)]
pub struct CityView {
    pub region: String,
    pub name: String,
    pub owner_id: Option<Uuid>,
    pub owner_name: Option<String>,
    #[serde(with = "time::serde::rfc3339::option")]
    pub owner_since: Option<OffsetDateTime>,
    pub price: i64,
    /// How much the current owner paid.
    pub paid: i64,
    /// Auction in progress (city sale).
    pub auction: Option<AuctionView>,
    pub treasury: i64,
    pub tax: f32,
    pub tax_min: f64,
    pub tax_max: f64,
    pub buy: Vec<PriceView>,
    pub sell: Vec<PriceView>,
    pub stations: Vec<StationRow>,
    pub stock: BTreeMap<String, i64>,
    pub buffs: Vec<(String, String)>,
    pub weapons: Vec<WeaponRow>,
    /// City defense = sum of the weapon tiers (finished ones).
    pub defense: i32,
    #[serde(with = "time::serde::rfc3339::option")]
    pub next_attack_at: Option<OffsetDateTime>,
    pub last_attack: Option<Value>,
    pub week: String,
    pub this_week: Value,
}

#[derive(Serialize)]
pub struct AuctionView {
    pub start: i64,
    #[serde(with = "time::serde::rfc3339")]
    pub ends_at: OffsetDateTime,
    pub forced: bool,
    pub bid: Option<i64>,
    pub bidder_name: Option<String>,
    /// Lowest bid accepted now.
    pub min_bid: i64,
    /// The caller holds the highest bid.
    pub mine: bool,
}

fn min_bid(c: &CityRow) -> i64 {
    match c.auction_bid {
        Some(b) => b + ((b as f64 * RULES.leilao.incremento_minimo).ceil() as i64).max(1),
        None => c.auction_start.unwrap_or(0),
    }
}

#[derive(Deserialize)]
pub struct ViewQuery {
    pub character_id: Option<Uuid>,
}

pub async fn get(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Query(q): Query<ViewQuery>) -> ApiResult<Json<CityView>> {
    let mut tx = state.db.begin().await?;
    advance_city(&mut tx, &region, OffsetDateTime::now_utc()).await?;
    let v = view(&mut tx, &region, q.character_id).await?;
    tx.commit().await?;
    Ok(Json(v))
}

async fn view(tx: &mut sqlx::PgConnection, region: &str, who: Option<Uuid>) -> ApiResult<CityView> {
    let c = lock_city(tx, region).await?;
    let owner_name: Option<String> = match c.owner_id {
        Some(id) => sqlx::query_scalar("select name from characters where id = $1").bind(id).fetch_optional(&mut *tx).await?,
        None => None,
    };
    let stations = stations_of(tx, region).await?;
    let weapons = weapons_of(tx, region).await?;
    let defense = weapons.iter().map(|w| w.tier).sum();
    let stock: Vec<(String, i64)> = sqlx::query_as("select item_id, quantity from city_stock where region = $1 and quantity > 0 order by item_id")
        .bind(region)
        .fetch_all(&mut *tx)
        .await?;
    let buffs: Vec<(String, String)> = sqlx::query_as("select station, buff from city_buffs where region = $1 and week = $2 order by station, buff")
        .bind(region)
        .bind(&c.week)
        .fetch_all(&mut *tx)
        .await?;
    let mut used: HashMap<String, (i64, i64)> = HashMap::new();
    if let Some(who) = who {
        let rows: Vec<(String, i64, i64)> = sqlx::query_as("select item_id, sold, bought from city_limits where region = $1 and week = $2 and character_id = $3")
            .bind(region)
            .bind(&c.week)
            .bind(who)
            .fetch_all(&mut *tx)
            .await?;
        used = rows.into_iter().map(|(i, s, b)| (i, (s, b))).collect();
    }
    let this_week: Option<Value> = sqlx::query_scalar(
        "select to_jsonb(w) - 'region' - 'week' from city_weeks w where region = $1 and week = $2",
    )
    .bind(region)
    .bind(&c.week)
    .fetch_optional(&mut *tx)
    .await?;
    let lim = &RULES.limite_semanal;
    Ok(CityView {
        region: c.region.clone(),
        name: c.name.clone(),
        owner_id: c.owner_id,
        owner_name,
        owner_since: c.owner_since,
        price: RULES.preco_cidade,
        paid: c.paid,
        auction: match (c.auction_start, c.auction_ends_at) {
            (Some(start), Some(ends_at)) => Some(AuctionView {
                start,
                ends_at,
                forced: c.auction_forced,
                bid: c.auction_bid,
                bidder_name: match c.auction_bidder {
                    Some(id) => sqlx::query_scalar("select name from characters where id = $1").bind(id).fetch_optional(&mut *tx).await?,
                    None => None,
                },
                min_bid: min_bid(&c),
                mine: who.is_some() && c.auction_bidder == who,
            }),
            _ => None,
        },
        treasury: c.treasury,
        tax: c.tax,
        tax_min: RULES.imposto.min,
        tax_max: RULES.imposto.max,
        buy: PRICES.compra.iter().map(|(i, r)| PriceView {
            item_id: i.clone(),
            price: price_of(&c.buy_prices, i, r),
            range: *r,
            used: used.get(i).map(|u| u.0).unwrap_or(0),
            limit: r.limite.unwrap_or(lim.compra_por_materia_prima),
        }).collect(),
        sell: PRICES.venda.iter().map(|(i, r)| PriceView {
            item_id: i.clone(),
            price: price_of(&c.sell_prices, i, r),
            range: *r,
            used: used.get(i).map(|u| u.1).unwrap_or(0),
            limit: r.limite.unwrap_or(lim.venda_por_item),
        }).collect(),
        stations,
        stock: stock.into_iter().collect(),
        buffs,
        weapons,
        defense,
        next_attack_at: c.next_attack_at,
        last_attack: c.last_attack,
        week: c.week,
        this_week: this_week.unwrap_or(json!({})),
    })
}

#[derive(Deserialize)]
pub struct LogQuery {
    pub limit: Option<i64>,
}

pub async fn log_list(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Query(q): Query<LogQuery>) -> ApiResult<Json<Vec<Value>>> {
    let rows: Vec<Value> = sqlx::query_scalar(
        "select to_jsonb(l) from city_log l where region = $1 order by serial desc limit $2",
    )
    .bind(&region)
    .bind(q.limit.unwrap_or(100).clamp(1, 1000))
    .fetch_all(&state.db)
    .await?;
    Ok(Json(rows))
}

// ---------------------------------------------------------------- players: sell raw materials, buy from the list

#[derive(Deserialize)]
pub struct Trade {
    pub character_id: Uuid,
    pub item_id: String,
    pub quantity: i64,
    /// Unit price the player saw; a different current price = 409 `price_changed`.
    pub unit_price: i64,
    /// Bag and coins after the trade (game server).
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

/// The player sells raw materials to the city: paid from the treasury, added to the stock.
pub async fn sell_raw(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(t): Json<Trade>) -> ApiResult<Json<CityView>> {
    let Some(range) = PRICES.compra.get(&t.item_id) else {
        return Err(ApiError::bad_request("not_bought", "A cidade não compra este item."));
    };
    if t.quantity <= 0 {
        return Err(ApiError::bad_request("invalid_quantity", "Quantidade inválida."));
    }
    t.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    advance_city(&mut tx, &region, OffsetDateTime::now_utc()).await?;
    let c = lock_city(&mut tx, &region).await?;
    let mut price = price_of(&c.buy_prices, &t.item_id, range);
    if c.owner_id == Some(t.character_id) {
        price = price.min(range.padrao);   // the governor does not pay himself more than the default
    }
    if price != t.unit_price {
        return Err(ApiError::conflict("price_changed", "O preço mudou."));
    }
    let used = limit_add(&mut tx, &region, &c.week, t.character_id, &t.item_id, t.quantity, 0).await?;
    if used > range.limite.unwrap_or(RULES.limite_semanal.compra_por_materia_prima) {
        return Err(ApiError::conflict("city_limit", "Limite da semana para este item."));
    }
    let total = price * t.quantity;
    set_treasury(&mut tx, &region, -total).await?;
    let ctx = t.snapshot.apply(&mut tx, t.character_id).await?;
    stock_add(&mut tx, &region, &t.item_id, t.quantity).await?;
    week_add(&mut tx, &region, &c.week, "raw_units", t.quantity).await?;
    week_add(&mut tx, &region, &c.week, "raw_coins", total).await?;
    journal::log(&mut tx, &ctx, &format!("city:{region}"), Some(&t.item_id), t.quantity, 0, 0, &empty_props()).await?;
    clog(&mut tx, &region, "raw_bought", Some(t.character_id), Some(&t.item_id), t.quantity, total, json!({"unit_price": price})).await?;
    let v = view(&mut tx, &region, Some(t.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

/// The player buys an item from the list: goes into the treasury.
pub async fn buy_item(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(t): Json<Trade>) -> ApiResult<Json<CityView>> {
    let Some(range) = PRICES.venda.get(&t.item_id) else {
        return Err(ApiError::bad_request("not_sold", "A cidade não vende este item."));
    };
    if t.quantity <= 0 {
        return Err(ApiError::bad_request("invalid_quantity", "Quantidade inválida."));
    }
    t.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    advance_city(&mut tx, &region, OffsetDateTime::now_utc()).await?;
    let c = lock_city(&mut tx, &region).await?;
    let price = price_of(&c.sell_prices, &t.item_id, range);
    if price != t.unit_price {
        return Err(ApiError::conflict("price_changed", "O preço mudou."));
    }
    let used = limit_add(&mut tx, &region, &c.week, t.character_id, &t.item_id, 0, t.quantity).await?;
    if used > range.limite.unwrap_or(RULES.limite_semanal.venda_por_item) {
        return Err(ApiError::conflict("city_limit", "Limite da semana para este item."));
    }
    let total = price * t.quantity;
    let ctx = t.snapshot.apply(&mut tx, t.character_id).await?;
    set_treasury(&mut tx, &region, total).await?;
    week_add(&mut tx, &region, &c.week, "list_units", t.quantity).await?;
    week_add(&mut tx, &region, &c.week, "list_coins", total).await?;
    journal::log(&mut tx, &ctx, &format!("city:{region}"), Some(&t.item_id), -t.quantity, 0, 0, &empty_props()).await?;
    clog(&mut tx, &region, "list_sold", Some(t.character_id), Some(&t.item_id), t.quantity, total, json!({"unit_price": price})).await?;
    let v = view(&mut tx, &region, Some(t.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

/// Adds to the weekly limit; returns the total of the side that changed.
async fn limit_add(tx: &mut sqlx::PgConnection, region: &str, week: &str, who: Uuid, item: &str, sold: i64, bought: i64) -> ApiResult<i64> {
    let (s, b): (i64, i64) = sqlx::query_as(
        "insert into city_limits (region, week, character_id, item_id, sold, bought) values ($1, $2, $3, $4, $5, $6)
         on conflict (region, week, character_id, item_id) do update set sold = city_limits.sold + $5, bought = city_limits.bought + $6
         returning sold, bought",
    )
    .bind(region)
    .bind(week)
    .bind(who)
    .bind(item)
    .bind(sold)
    .bind(bought)
    .fetch_one(&mut *tx)
    .await?;
    Ok(if sold > 0 { s } else { b })
}

async fn stock_add(tx: &mut sqlx::PgConnection, region: &str, item: &str, delta: i64) -> ApiResult<()> {
    if delta >= 0 {
        sqlx::query(
            "insert into city_stock (region, item_id, quantity) values ($1, $2, $3)
             on conflict (region, item_id) do update set quantity = city_stock.quantity + $3",
        )
        .bind(region)
        .bind(item)
        .bind(delta)
        .execute(&mut *tx)
        .await?;
        return Ok(());
    }
    let r = sqlx::query("update city_stock set quantity = quantity + $3 where region = $1 and item_id = $2 and quantity + $3 >= 0")
        .bind(region)
        .bind(item)
        .bind(delta)
        .execute(&mut *tx)
        .await?;
    if r.rows_affected() == 0 {
        return Err(ApiError::conflict("city_no_stock", "A cidade não tem material para isso."));
    }
    Ok(())
}

async fn stock_of(tx: &mut sqlx::PgConnection, region: &str, item: &str) -> ApiResult<i64> {
    Ok(sqlx::query_scalar("select quantity from city_stock where region = $1 and item_id = $2")
        .bind(region)
        .bind(item)
        .fetch_optional(&mut *tx)
        .await?
        .unwrap_or(0))
}

/// Crafting tax (comes with the character save, same transaction): goes into the treasury and counts the craft
/// at the station (upkeep).
#[derive(Deserialize, Debug, Clone)]
pub struct CityTax {
    pub region: String,
    pub station: String,
    pub amount: i64,
}

pub(crate) async fn apply_tax(tx: &mut sqlx::PgConnection, who: Uuid, t: &CityTax) -> ApiResult<()> {
    if t.amount < 0 {
        return Err(ApiError::bad_request("invalid_tax", "Imposto negativo."));
    }
    let c = lock_city(tx, &t.region).await?;
    if t.amount > 0 {
        set_treasury(tx, &t.region, t.amount).await?;
        week_add(tx, &t.region, &c.week, "tax_coins", t.amount).await?;
    }
    sqlx::query("update city_stations set crafts = crafts + 1 where region = $1 and station = $2")
        .bind(&t.region)
        .bind(&t.station)
        .execute(&mut *tx)
        .await?;
    clog(tx, &t.region, "tax", Some(who), None, 1, t.amount, json!({"station": t.station})).await?;
    Ok(())
}

// ---------------------------------------------------------------- owner: buy, sell, manage, projects

#[derive(Deserialize)]
pub struct BuyCity {
    pub character_id: Uuid,
    /// Price the player saw (initial or the owner's asking price).
    pub price: i64,
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

pub async fn buy_city(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(b): Json<BuyCity>) -> ApiResult<Json<CityView>> {
    b.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    let now = OffsetDateTime::now_utc();
    advance_city(&mut tx, &region, now).await?;
    let c = lock_city(&mut tx, &region).await?;
    // City without owner: fixed price, the amount is removed from the game. With owner, only by auction (`bid`).
    if c.owner_id.is_some() {
        return Err(ApiError::conflict("city_not_for_sale", "A cidade tem governador; a venda é por leilão."));
    }
    let asked = RULES.preco_cidade;
    if asked != b.price {
        return Err(ApiError::conflict("price_changed", "O preço mudou."));
    }
    let ctx = b.snapshot.apply(&mut tx, b.character_id).await?;
    set_owner(&mut tx, &c, b.character_id, asked, now).await?;
    journal::log(&mut tx, &ctx, &format!("city:{region}"), None, 0, 0, 0, &json!({"bought_city": asked})).await?;
    clog(&mut tx, &region, "owner", Some(b.character_id), None, 0, asked, json!({"from": null, "auction": false})).await?;
    let v = view(&mut tx, &region, Some(b.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

/// Owner change (purchase with no owner or end of auction): attack timer restarts, the week becomes theirs.
async fn set_owner(tx: &mut sqlx::PgConnection, c: &CityRow, owner: Uuid, paid: i64, now: OffsetDateTime) -> ApiResult<()> {
    sqlx::query(
        "update cities set owner_id = $2, owner_since = $3, paid = $4, next_attack_at = $5,
                auction_start = null, auction_ends_at = null, auction_forced = false, auction_bid = null, auction_bidder = null
          where region = $1",
    )
    .bind(&c.region)
    .bind(owner)
    .bind(now)
    .bind(paid)
    .bind(now + Duration::days(RULES.ataque.primeiro_apos_dias))
    .execute(&mut *tx)
    .await?;
    sqlx::query("update city_weeks set owner_id = $3 where region = $1 and week = $2")
        .bind(&c.region)
        .bind(&c.week)
        .bind(owner)
        .execute(&mut *tx)
        .await?;
    Ok(())
}

#[derive(Deserialize)]
pub struct Sale {
    pub character_id: Uuid,
    /// Auction starting bid; null cancels (only with no bids and outside a forced sale).
    pub price: Option<i64>,
}

/// The governor puts the city up for a 24 h auction with any starting bid (after the grace period).
pub async fn sale(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(s): Json<Sale>) -> ApiResult<Json<CityView>> {
    let mut tx = state.db.begin().await?;
    let now = OffsetDateTime::now_utc();
    advance_city(&mut tx, &region, now).await?;
    let c = lock_city(&mut tx, &region).await?;
    require_owner(&c, s.character_id)?;
    if c.auction_forced {
        return Err(ApiError::conflict("forced_sale", "A cidade está em leilão obrigatório."));
    }
    match s.price {
        Some(p) => {
            if p <= 0 {
                return Err(ApiError::bad_request("invalid_price", "Preço inválido."));
            }
            if c.auction_start.is_some() {
                return Err(ApiError::conflict("auction_running", "Já há um leilão."));
            }
            if c.owner_since.is_some_and(|t| now - t < Duration::days(RULES.carencia_dias)) {
                return Err(ApiError::conflict("city_grace", "Ainda na carência depois da compra."));
            }
            start_auction(&mut tx, &region, p, false, now).await?;
        }
        None => {
            if c.auction_bid.is_some() {
                return Err(ApiError::conflict("auction_has_bids", "O leilão já tem lance."));
            }
            sqlx::query("update cities set auction_start = null, auction_ends_at = null where region = $1").bind(&region).execute(&mut *tx).await?;
        }
    }
    clog(&mut tx, &region, "sale", Some(s.character_id), None, 0, s.price.unwrap_or(0), json!({})).await?;
    let v = view(&mut tx, &region, Some(s.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

async fn start_auction(tx: &mut sqlx::PgConnection, region: &str, start: i64, forced: bool, now: OffsetDateTime) -> ApiResult<()> {
    sqlx::query(
        "update cities set auction_start = $2, auction_ends_at = $3, auction_forced = $4, auction_bid = null, auction_bidder = null
          where region = $1",
    )
    .bind(region)
    .bind(start)
    .bind(now + Duration::hours(RULES.leilao.horas))
    .bind(forced)
    .execute(&mut *tx)
    .await?;
    clog(tx, region, "auction", None, None, 0, start, json!({"forced": forced})).await?;
    Ok(())
}

#[derive(Deserialize)]
pub struct Bid {
    pub character_id: Uuid,
    pub amount: i64,
    /// Bag and coins already without the bid (held until someone outbids it or the auction ends).
    #[serde(flatten)]
    pub snapshot: Snapshot,
}

pub async fn bid(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(b): Json<Bid>) -> ApiResult<Json<CityView>> {
    b.snapshot.validate()?;
    let mut tx = state.db.begin().await?;
    let now = OffsetDateTime::now_utc();
    advance_city(&mut tx, &region, now).await?;
    let c = lock_city(&mut tx, &region).await?;
    if c.auction_start.is_none() {
        return Err(ApiError::conflict("no_auction", "A cidade não está em leilão."));
    }
    if c.owner_id == Some(b.character_id) {
        return Err(ApiError::conflict("own_city", "O governador não dá lance na própria cidade."));
    }
    if b.amount < min_bid(&c) {
        return Err(ApiError::conflict("bid_too_low", "Lance abaixo do mínimo."));
    }
    let ctx = b.snapshot.apply(&mut tx, b.character_id).await?;
    // The outbid amount goes back as credit to the bidder.
    if let (Some(prev), Some(who)) = (c.auction_bid, c.auction_bidder) {
        credit(&mut tx, who, prev, "city_bid_refund", None).await?;
    }
    sqlx::query("update cities set auction_bid = $2, auction_bidder = $3 where region = $1")
        .bind(&region)
        .bind(b.amount)
        .bind(b.character_id)
        .execute(&mut *tx)
        .await?;
    journal::log(&mut tx, &ctx, &format!("city:{region}"), None, 0, 0, 0, &json!({"bid": b.amount})).await?;
    clog(&mut tx, &region, "bid", Some(b.character_id), None, 0, b.amount, json!({})).await?;
    let v = view(&mut tx, &region, Some(b.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

/// End of auction: the highest bid wins (the outgoing governor receives the bid, minus the fee if the sale was theirs).
/// No bid: an owner's sale ends with no change; a forced sale leaves the city without a governor.
async fn settle(tx: &mut sqlx::PgConnection, region: &str, now: OffsetDateTime) -> ApiResult<()> {
    let c: CityRow = sqlx::query_as("select * from cities where region = $1 for update").bind(region).fetch_one(&mut *tx).await?;
    let Some(ends) = c.auction_ends_at else {
        return Ok(());
    };
    if ends > now {
        return Ok(());
    }
    match (c.auction_bid, c.auction_bidder) {
        (Some(amount), Some(winner)) => {
            if let Some(seller) = c.owner_id {
                let net = if c.auction_forced { amount } else { amount - (amount as f64 * RULES.taxa_revenda).round() as i64 };
                credit(tx, seller, net, "city_sold", None).await?;
            }
            set_owner(tx, &c, winner, amount, ends).await?;
            clog(tx, region, "owner", Some(winner), None, 0, amount, json!({"from": c.owner_id, "auction": true, "forced": c.auction_forced})).await?;
        }
        _ if c.auction_forced => {
            sqlx::query(
                "update cities set owner_id = null, owner_since = null, next_attack_at = null,
                        auction_start = null, auction_ends_at = null, auction_forced = false where region = $1",
            )
            .bind(region)
            .execute(&mut *tx)
            .await?;
            clog(tx, region, "owner", None, None, 0, 0, json!({"from": c.owner_id, "auction": true, "forced": true, "no_bids": true})).await?;
        }
        _ => {
            sqlx::query("update cities set auction_start = null, auction_ends_at = null where region = $1").bind(region).execute(&mut *tx).await?;
            clog(tx, region, "auction_end", c.owner_id, None, 0, 0, json!({"no_bids": true})).await?;
        }
    }
    Ok(())
}

#[derive(Deserialize)]
pub struct Manage {
    pub character_id: Uuid,
    #[serde(default)]
    pub buy_prices: Option<BTreeMap<String, i64>>,
    #[serde(default)]
    pub sell_prices: Option<BTreeMap<String, i64>>,
    #[serde(default)]
    pub tax: Option<f64>,
}

fn check_prices(p: &BTreeMap<String, i64>, ranges: &BTreeMap<String, PriceRange>) -> ApiResult<()> {
    for (item, price) in p {
        let Some(r) = ranges.get(item) else {
            return Err(ApiError::bad_request("unknown_item", format!("Item fora da tabela: {item}")));
        };
        if *price < r.min || *price > r.max {
            return Err(ApiError::bad_request("price_out_of_range", format!("Preço fora da faixa: {item}")));
        }
    }
    Ok(())
}

pub async fn manage(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(m): Json<Manage>) -> ApiResult<Json<CityView>> {
    if let Some(p) = &m.buy_prices {
        check_prices(p, &PRICES.compra)?;
    }
    if let Some(p) = &m.sell_prices {
        check_prices(p, &PRICES.venda)?;
    }
    if m.tax.is_some_and(|t| !(RULES.imposto.min..=RULES.imposto.max).contains(&t)) {
        return Err(ApiError::bad_request("tax_out_of_range", "Imposto fora da faixa."));
    }
    let mut tx = state.db.begin().await?;
    advance_city(&mut tx, &region, OffsetDateTime::now_utc()).await?;
    let c = lock_city(&mut tx, &region).await?;
    require_owner(&c, m.character_id)?;
    let merge = |old: &Value, new: &Option<BTreeMap<String, i64>>| -> Value {
        let mut o = old.as_object().cloned().unwrap_or_default();
        if let Some(n) = new {
            for (k, v) in n {
                o.insert(k.clone(), json!(v));
            }
        }
        Value::Object(o)
    };
    sqlx::query("update cities set buy_prices = $2, sell_prices = $3, tax = coalesce($4, tax) where region = $1")
        .bind(&region)
        .bind(merge(&c.buy_prices, &m.buy_prices))
        .bind(merge(&c.sell_prices, &m.sell_prices))
        .bind(m.tax.map(|t| t as f32))
        .execute(&mut *tx)
        .await?;
    clog(&mut tx, &region, "manage", Some(m.character_id), None, 0, 0, json!({"buy": m.buy_prices, "sell": m.sell_prices, "tax": m.tax})).await?;
    let v = view(&mut tx, &region, Some(m.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

#[derive(Deserialize)]
pub struct Project {
    pub character_id: Uuid,
    /// "station" (tier, level or repair, depending on the station), "buff" or "weapon" (weapon tier).
    pub kind: String,
    #[serde(default)]
    pub station: Option<String>,
    #[serde(default)]
    pub buff: Option<String>,
    #[serde(default)]
    pub weapon: Option<String>,
}

/// Pays an upgrade: basket from stock (fraction `share` of it) and coins; returns the value (range default) of the basket paid.
async fn pay(tx: &mut sqlx::PgConnection, region: &str, up: &Up, share: f64, coins: bool) -> ApiResult<i64> {
    let mut value = 0;
    for (item, qty) in &up.materiais {
        let q = (*qty as f64 * share).round() as i64;
        stock_add(tx, region, item, -q).await?;
        value += q * PRICES.compra.get(item).map(|r| r.padrao).unwrap_or(0);
    }
    if coins && up.moedas > 0 {
        set_treasury(tx, region, -up.moedas).await?;
        value += up.moedas;
    }
    Ok(value)
}

pub async fn project(State(state): State<AppState>, _server: GameServer, Path(region): Path<String>, Json(p): Json<Project>) -> ApiResult<Json<CityView>> {
    let mut tx = state.db.begin().await?;
    let now = OffsetDateTime::now_utc();
    advance_city(&mut tx, &region, now).await?;
    let c = lock_city(&mut tx, &region).await?;
    require_owner(&c, p.character_id)?;
    let ends = now + Duration::hours(RULES.obra_horas);
    match p.kind.as_str() {
        "weapon" => {
            // Fort weapon: goes up one tier (up to the last one in the rules), 24 h of work.
            let Some(weapon) = p.weapon.as_ref().filter(|w| RULES.armas.tipos.contains(w)) else {
                return Err(ApiError::bad_request("invalid_project", "Arma desconhecida."));
            };
            let w: WeaponRow = sqlx::query_as("select weapon, tier, work_target, work_ends_at from city_weapons where region = $1 and weapon = $2 for update")
                .bind(&region)
                .bind(weapon)
                .fetch_one(&mut *tx)
                .await?;
            if w.work_ends_at.is_some() {
                return Err(ApiError::conflict("work_running", "Já há uma obra nesta arma."));
            }
            let Some(up) = RULES.armas.subir.iter().find(|u| u.para == Some(w.tier + 1)) else {
                return Err(ApiError::conflict("weapon_max", "A arma já está na escala máxima."));
            };
            let value = pay(&mut tx, &region, up, 1.0, true).await?;
            sqlx::query("update city_weapons set work_target = $3, work_ends_at = $4 where region = $1 and weapon = $2")
                .bind(&region)
                .bind(weapon)
                .bind(w.tier + 1)
                .bind(ends)
                .execute(&mut *tx)
                .await?;
            week_add(&mut tx, &region, &c.week, "project_value", value).await?;
            clog(&mut tx, &region, "weapon", Some(p.character_id), None, 1, up.moedas, json!({"weapon": weapon, "tier": w.tier + 1})).await?;
        }
        "buff" => {
            let (Some(station), Some(buff)) = (&p.station, &p.buff) else {
                return Err(ApiError::bad_request("invalid_project", "Buff sem mesa ou sem tipo."));
            };
            let Some(rule) = RULES.buffs.get(buff) else {
                return Err(ApiError::bad_request("invalid_project", "Buff desconhecido."));
            };
            if !RULES.mesas.contains_key(station) {
                return Err(ApiError::bad_request("invalid_project", "Mesa desconhecida."));
            }
            let r = sqlx::query("insert into city_buffs (region, week, station, buff) values ($1, $2, $3, $4) on conflict do nothing")
                .bind(&region)
                .bind(&c.week)
                .bind(station)
                .bind(buff)
                .execute(&mut *tx)
                .await?;
            if r.rows_affected() == 0 {
                return Err(ApiError::conflict("buff_active", "Este buff já está ativo nesta mesa."));
            }
            set_treasury(&mut tx, &region, -rule.custo).await?;
            week_add(&mut tx, &region, &c.week, "buffs", 1).await?;
            week_add(&mut tx, &region, &c.week, "project_value", rule.custo).await?;
            clog(&mut tx, &region, "buff", Some(p.character_id), None, 1, rule.custo, json!({"station": station, "buff": buff})).await?;
        }
        "station" => {
            // Station: first the tier (up to 5), then the levels; a level the station already had is a repair (30% of
            // the basket, no coins, outside the goal). All with 24 h of work.
            let Some(station) = p.station.as_ref().filter(|st| RULES.mesas.contains_key(*st)) else {
                return Err(ApiError::bad_request("invalid_project", "Mesa desconhecida."));
            };
            let s: StationRow = sqlx::query_as(sqlx::AssertSqlSafe(format!("select {STATION_COLS} from city_stations where region = $1 and station = $2 for update")))
                .bind(&region)
                .bind(station)
                .fetch_one(&mut *tx)
                .await?;
            if s.work.is_some() {
                return Err(ApiError::conflict("work_running", "Já há uma obra nesta mesa."));
            }
            let (work, target, up) = if s.tier < TOP_TIER {
                let Some(up) = RULES.mesa_escala.subir.iter().find(|u| u.para == Some(s.tier + 1)) else {
                    return Err(ApiError::conflict("station_max", "Sem regra para a próxima escala."));
                };
                ("tier", s.tier + 1, up)
            } else {
                if s.level >= RULES.mesa_nivel.max {
                    return Err(ApiError::conflict("station_max", "A mesa já está no nível máximo."));
                }
                let Some(up) = RULES.mesa_nivel.subir.iter().find(|u| u.nivel == Some(s.level + 1)) else {
                    return Err(ApiError::conflict("station_max", "Sem regra para o próximo nível."));
                };
                (if s.level < s.reached { "repair" } else { "level" }, s.level + 1, up)
            };
            let repair = work == "repair";
            let value = pay(&mut tx, &region, up, if repair { RULES.conserto_fracao } else { 1.0 }, !repair).await?;
            sqlx::query("update city_stations set work = $3, work_target = $4, work_ends_at = $5 where region = $1 and station = $2")
                .bind(&region)
                .bind(station)
                .bind(work)
                .bind(target)
                .bind(ends)
                .execute(&mut *tx)
                .await?;
            week_add(&mut tx, &region, &c.week, if repair { "repair_value" } else { "project_value" }, value).await?;
            clog(&mut tx, &region, work, Some(p.character_id), None, 0, if repair { 0 } else { up.moedas },
                json!({"station": station, "target": target})).await?;
        }
        _ => return Err(ApiError::bad_request("invalid_project", "Projeto desconhecido.")),
    }
    let v = view(&mut tx, &region, Some(p.character_id)).await?;
    tx.commit().await?;
    Ok(Json(v))
}

/// Finished works: the station or weapon goes up to the target.
async fn works(tx: &mut sqlx::PgConnection, region: &str, now: OffsetDateTime) -> ApiResult<()> {
    let done: Vec<(String, String, i32)> = sqlx::query_as(
        "update city_stations set
             tier = case when work = 'tier' then work_target else tier end,
             level = case when work in ('level', 'repair') then work_target else level end,
             reached = case when work in ('level', 'repair') then greatest(reached, work_target) else reached end,
             work = null, work_target = null, work_ends_at = null
          where region = $1 and work_ends_at <= $2
          returning station, 'mesa', tier + level",
    )
    .bind(region)
    .bind(now)
    .fetch_all(&mut *tx)
    .await?;
    for (station, _, _) in &done {
        clog(tx, region, "work_done", None, None, 0, 0, json!({"station": station})).await?;
    }
    let armed: Vec<(String, i32)> = sqlx::query_as(
        "update city_weapons set tier = work_target, work_target = null, work_ends_at = null
          where region = $1 and work_ends_at <= $2 returning weapon, tier",
    )
    .bind(region)
    .bind(now)
    .fetch_all(&mut *tx)
    .await?;
    for (weapon, tier) in &armed {
        clog(tx, region, "work_done", None, None, 0, 0, json!({"weapon": weapon, "tier": tier})).await?;
    }
    Ok(())
}

// ---------------------------------------------------------------- time: attacks and week rollover

#[derive(Deserialize, Default)]
pub struct Advance {
    /// Instant to use (tests); missing = now.
    #[serde(default, with = "time::serde::rfc3339::option")]
    pub now: Option<OffsetDateTime>,
}

/// Runs what is due in every city (nw-core calls it every minute; tests call it with `now`).
pub async fn advance(State(state): State<AppState>, _server: GameServer, body: Option<Json<Advance>>) -> ApiResult<StatusCode> {
    let now = body.and_then(|b| b.0.now).unwrap_or_else(OffsetDateTime::now_utc);
    advance_all(&state.db, now).await?;
    Ok(StatusCode::NO_CONTENT)
}

pub async fn advance_all(db: &sqlx::PgPool, now: OffsetDateTime) -> ApiResult<()> {
    let mut tx = db.begin().await?;
    for r in &RULES.regioes {
        lock_city(&mut tx, &r.id).await?;
    }
    rollover(&mut tx, now).await?;
    for r in &RULES.regioes {
        works(&mut tx, &r.id, now).await?;
        attacks(&mut tx, &r.id, now).await?;
        settle(&mut tx, &r.id, now).await?;
    }
    tx.commit().await?;
    Ok(())
}

/// Before any action in a city: week and attacks up to date (the rollover compares all cities).
async fn advance_city(tx: &mut sqlx::PgConnection, region: &str, now: OffsetDateTime) -> ApiResult<()> {
    let c = lock_city(tx, region).await?;
    if c.week != week_id(now) {
        for r in &RULES.regioes {
            lock_city(tx, &r.id).await?;
        }
        rollover(tx, now).await?;
    }
    works(tx, region, now).await?;
    attacks(tx, region, now).await?;
    settle(tx, region, now).await
}

fn rand01() -> f64 {
    let mut b = [0u8; 8];
    rand::fill(&mut b);
    (u64::from_le_bytes(b) >> 11) as f64 / (1u64 << 53) as f64
}

async fn attacks(tx: &mut sqlx::PgConnection, region: &str, now: OffsetDateTime) -> ApiResult<()> {
    loop {
        let c: CityRow = sqlx::query_as("select * from cities where region = $1 for update").bind(region).fetch_one(&mut *tx).await?;
        let (Some(_owner), Some(at), Some(since)) = (c.owner_id, c.next_attack_at, c.owner_since) else {
            return Ok(());
        };
        if at > now {
            return Ok(());
        }
        let mut stations = stations_of(tx, region).await?;
        stations.sort_by(|x, y| y.level.cmp(&x.level).then(x.station.cmp(&y.station)));
        let weapons = weapons_of(tx, region).await?;
        let defense: i32 = weapons.iter().map(|w| w.tier).sum();
        let a = &RULES.ataque;
        let weeks = ((at - since).whole_days() / 7) as f64;
        // Strength grows with the owner's week and the city's size (tiers above the initial one, and levels).
        let growth: i32 = stations.iter().map(|s| (s.tier - RULES.mesa_escala.inicial).max(0) + s.level).sum();
        let strength = (a.base + a.por_semana * weeks + a.por_nivel * growth as f64) * (1.0 - a.variacao + 2.0 * a.variacao * rand01());
        let won = defense as f64 >= strength;
        let mut lost = Vec::new();
        if !won {
            // Stations lose levels (the tier never drops); with no level to lose, a weapon loses a tier.
            for s in stations.iter().filter(|s| s.level > 0).take(a.niveis_perdidos) {
                sqlx::query("update city_stations set level = level - 1 where region = $1 and station = $2")
                    .bind(region)
                    .bind(&s.station)
                    .execute(&mut *tx)
                    .await?;
                lost.push(s.station.clone());
            }
            if lost.is_empty() {
                if let Some(w) = weapons.iter().filter(|w| w.tier > RULES.armas.inicial).max_by_key(|w| w.tier) {
                    sqlx::query("update city_weapons set tier = tier - 1 where region = $1 and weapon = $2")
                        .bind(region)
                        .bind(&w.weapon)
                        .execute(&mut *tx)
                        .await?;
                    lost.push(w.weapon.clone());
                }
            }
        }
        let detail = json!({"at": at.unix_timestamp(), "strength": strength, "defense": defense, "won": won, "lost": lost});
        sqlx::query("update cities set next_attack_at = $2, last_attack = $3 where region = $1")
            .bind(region)
            .bind(at + Duration::hours(a.intervalo_horas))
            .bind(&detail)
            .execute(&mut *tx)
            .await?;
        clog(tx, region, "attack", None, None, 0, 0, detail).await?;
    }
}

#[derive(sqlx::FromRow)]
struct WeekRow {
    region: String,
    raw_units: i64,
    raw_coins: i64,
    repair_value: i64,
    list_units: i64,
    project_value: i64,
    buffs: i32,
}

/// Closes the due weeks of all cities (the rows are already locked): upkeep, goal, tier, prize.
async fn rollover(tx: &mut sqlx::PgConnection, now: OffsetDateTime) -> ApiResult<()> {
    let current = week_id(now);
    loop {
        let cities: Vec<CityRow> = sqlx::query_as("select * from cities where week < $1 order by region").bind(&current).fetch_all(&mut *tx).await?;
        let Some(oldest) = cities.iter().map(|c| c.week.clone()).min() else {
            return Ok(());
        };
        let closing: Vec<&CityRow> = cities.iter().filter(|c| c.week == oldest).collect();
        let mut results: Vec<(String, Option<Uuid>, f64, bool)> = Vec::new();
        for c in &closing {
            upkeep(tx, &c.region).await?;
            let w: WeekRow = sqlx::query_as("select region, raw_units, raw_coins, repair_value, list_units, project_value, buffs from city_weeks where region = $1 and week = $2")
                .bind(&c.region)
                .bind(&c.week)
                .fetch_one(&mut *tx)
                .await?;
            let avg_level: f64 = sqlx::query_scalar("select coalesce(avg(greatest(tier - 2, 0) + level), 0)::float8 from city_stations where region = $1")
                .bind(&c.region)
                .fetch_one(&mut *tx)
                .await?;
            let pr = &RULES.premio;
            let score = (w.raw_coins - w.repair_value + w.project_value) as f64 + pr.peso_nivel as f64 * avg_level;
            let teneb = w.raw_units < pr.tenebroso_materia_prima || w.list_units < pr.tenebroso_lista || w.buffs == 0;
            results.push((w.region, c.owner_id, score, teneb));
        }
        // Mean of the cities that did not end in `tenebroso`; a single city = mean of its last 4 weeks.
        let alive: Vec<f64> = results.iter().filter(|r| !r.3).map(|r| r.2).collect();
        let best = results.iter().filter(|r| !r.3).map(|r| r.2).fold(f64::MIN, f64::max);
        for (region, owner, score, teneb) in &results {
            let mean = if alive.len() > 1 {
                alive.iter().sum::<f64>() / alive.len() as f64
            } else {
                let hist: Vec<f64> = sqlx::query_scalar("select score from city_weeks where region = $1 and score is not null and tier <> 'tenebroso' order by week desc limit 3")
                    .bind(region)
                    .fetch_all(&mut *tx)
                    .await?;
                (hist.iter().sum::<f64>() + score) / (hist.len() + 1) as f64
            };
            let pr = &RULES.premio;
            let ratio = if mean > 0.0 { score / mean } else { 1.0 };
            let tier = if *teneb {
                "tenebroso"
            } else if ratio >= pr.faixas.excelente {
                if alive.len() > 1 && *score >= best { "melhor" } else { "excelente" }
            } else if ratio >= pr.faixas.muito_bom {
                "muito_bom"
            } else if ratio >= pr.faixas.bom {
                "bom"
            } else {
                "ruim"
            };
            let prize = match tier {
                "tenebroso" => pr.tenebroso,
                "ruim" => pr.ruim,
                "bom" => pr.bom,
                "muito_bom" => pr.muito_bom,
                "excelente" => pr.excelente,
                _ => pr.melhor,
            };
            let paid = if owner.is_some() { prize } else { 0 };
            sqlx::query("update city_weeks set score = $3, tier = $4, prize = $5 where region = $1 and week = $2")
                .bind(region)
                .bind(&oldest)
                .bind(score)
                .bind(owner.map(|_| tier))
                .bind(paid)
                .execute(&mut *tx)
                .await?;
            if let Some(owner) = owner {
                credit(tx, *owner, paid, "city_prize", None).await?;
                clog(tx, region, "prize", Some(*owner), None, 0, paid, json!({"week": oldest, "tier": tier, "score": score, "ratio": ratio})).await?;
                if *teneb {
                    // Forced 24 h auction from the initial price (replaces an owner's own auction with no bids).
                    let running: Option<i64> = sqlx::query_scalar("select auction_bid from cities where region = $1").bind(region).fetch_one(&mut *tx).await?;
                    if running.is_none() {
                        start_auction(tx, region, RULES.preco_cidade, true, now).await?;
                    }
                    clog(tx, region, "forced_sale", Some(*owner), None, 0, RULES.preco_cidade, json!({"week": oldest})).await?;
                }
            }
            // Next week.
            let next = week_begin(&oldest).map(|t| week_id(t + Duration::days(7))).unwrap_or_else(|| current.clone());
            sqlx::query("update cities set week = $2 where region = $1").bind(region).bind(&next).execute(&mut *tx).await?;
            sqlx::query("insert into city_weeks (region, week, owner_id) values ($1, $2, $3) on conflict do nothing")
                .bind(region)
                .bind(&next)
                .bind(owner)
                .execute(&mut *tx)
                .await?;
        }
    }
}

/// Weekly upkeep: each craft at an upgraded station consumes raw material per its level; without material, the station
/// loses a level. Resets the count.
async fn upkeep(tx: &mut sqlx::PgConnection, region: &str) -> ApiResult<()> {
    // Each craft at a station above the initial tier (or with a level) consumes the station's raw material; without
    // material, the station loses a level (the tier does not drop).
    let stations = stations_of(tx, region).await?;
    for s in stations {
        let size = (s.tier - RULES.mesa_escala.inicial).max(0) + s.level;
        let need = size as i64 * s.crafts as i64 * RULES.manutencao_por_fabricacao_nivel;
        if need == 0 {
            continue;
        }
        let material = &RULES.mesas[&s.station];
        let have = stock_of(tx, region, material).await?;
        if have >= need {
            sqlx::query("update city_stock set quantity = quantity - $3 where region = $1 and item_id = $2")
                .bind(region)
                .bind(material)
                .bind(need)
                .execute(&mut *tx)
                .await?;
            clog(tx, region, "upkeep", None, Some(material), need, 0, json!({"station": s.station})).await?;
        } else {
            if s.level > 0 {
                sqlx::query("update city_stations set level = level - 1 where region = $1 and station = $2")
                    .bind(region)
                    .bind(&s.station)
                    .execute(&mut *tx)
                    .await?;
            }
            clog(tx, region, "upkeep_failed", None, Some(material), need, 0, json!({"station": s.station, "had": have})).await?;
        }
    }
    sqlx::query("update city_stations set crafts = 0 where region = $1").bind(region).execute(&mut *tx).await?;
    Ok(())
}
