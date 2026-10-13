//! Fluxo completo contra Postgres real. `#[sqlx::test]` cria um banco temporário por teste a partir de
//! `DATABASE_URL` (ver README).

use std::sync::Arc;

use axum::body::Body;
use axum::http::{Request, StatusCode};
use http_body_util::BodyExt;
use serde_json::{json, Value};
use sqlx::PgPool;
use tower::ServiceExt;

use nw_core::{app, AppState};

const SECRET: &str = "segredo-do-servidor-de-teste";

struct Api {
    router: axum::Router,
}

impl Api {
    fn new(db: PgPool) -> Self {
        Self { router: app(AppState { db, server_secret: Arc::from(SECRET) }) }
    }

    async fn call(&self, method: &str, uri: &str, token: Option<&str>, server: bool, body: Option<Value>) -> (StatusCode, Value) {
        let mut req = Request::builder().method(method).uri(uri);
        if let Some(t) = token {
            req = req.header("authorization", format!("Bearer {t}"));
        }
        if server {
            req = req.header("x-nw-server-secret", SECRET);
        }
        let req = match body {
            Some(b) => req.header("content-type", "application/json").body(Body::from(b.to_string())),
            None => req.body(Body::empty()),
        }
        .unwrap();
        let res = self.router.clone().oneshot(req).await.unwrap();
        let status = res.status();
        let bytes = res.into_body().collect().await.unwrap().to_bytes();
        let value = serde_json::from_slice(&bytes).unwrap_or(Value::Null);
        (status, value)
    }

    async fn register(&self, db: &PgPool, name: &str) -> String {
        let invite = nw_core::routes::insert_invite(db, None, 7).await.unwrap();
        let (status, body) = self
            .call("POST", "/v1/auth/register", None, false, Some(json!({ "invite": invite.code, "name": name, "password": "senha-forte-123" })))
            .await;
        assert_eq!(status, StatusCode::CREATED, "{body}");
        body["token"].as_str().unwrap().to_string()
    }
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn convite_vale_uma_vez(db: PgPool) {
    let api = Api::new(db.clone());
    let invite = nw_core::routes::insert_invite(&db, None, 7).await.unwrap();
    let body = json!({ "invite": invite.code.to_lowercase().replace('-', ""), "name": "Paulo", "password": "senha-forte-123" });
    let (status, _) = api.call("POST", "/v1/auth/register", None, false, Some(body)).await;
    assert_eq!(status, StatusCode::CREATED);

    let body = json!({ "invite": invite.code, "name": "Outro", "password": "senha-forte-123" });
    let (status, err) = api.call("POST", "/v1/auth/register", None, false, Some(body)).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    assert_eq!(err["error"], "invalid_invite");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn login_e_logout(db: PgPool) {
    let api = Api::new(db.clone());
    api.register(&db, "Paulo").await;

    let (status, err) = api.call("POST", "/v1/auth/login", None, false, Some(json!({ "name": "paulo", "password": "errada-123" }))).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    assert_eq!(err["error"], "bad_credentials");
    let (status, _) = api.call("POST", "/v1/auth/login", None, false, Some(json!({ "name": "ninguem", "password": "errada-123" }))).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);

    let (status, body) = api.call("POST", "/v1/auth/login", None, false, Some(json!({ "name": "paulo", "password": "senha-forte-123" }))).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(body["account"]["name"], "Paulo");
    let token = body["token"].as_str().unwrap();

    let (status, _) = api.call("GET", "/v1/me", Some(token), false, None).await;
    assert_eq!(status, StatusCode::OK);
    let (status, _) = api.call("POST", "/v1/auth/logout", Some(token), false, None).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (status, _) = api.call("GET", "/v1/me", Some(token), false, None).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn nome_de_conta_e_personagem_unicos(db: PgPool) {
    let api = Api::new(db.clone());
    let a = api.register(&db, "Paulo").await;
    let invite = nw_core::routes::insert_invite(&db, None, 7).await.unwrap();
    let (status, err) = api
        .call("POST", "/v1/auth/register", None, false, Some(json!({ "invite": invite.code, "name": "PAULO", "password": "senha-forte-123" })))
        .await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "name_taken");

    let b = api.register(&db, "Joao").await;
    let (status, _) = api.call("POST", "/v1/characters", Some(&a), false, Some(json!({ "name": "Aranha" }))).await;
    assert_eq!(status, StatusCode::CREATED);
    let (status, err) = api.call("POST", "/v1/characters", Some(&b), false, Some(json!({ "name": "aranha" }))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "name_taken");
    let (status, _) = api.call("POST", "/v1/characters", Some(&b), false, Some(json!({ "name": "Ara nha" }))).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn limite_de_personagens(db: PgPool) {
    let api = Api::new(db.clone());
    let t = api.register(&db, "Paulo").await;
    for name in ["Um", "Dois", "Tres"] {
        let (status, body) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": name }))).await;
        if name == "Um" {
            assert_eq!(status, StatusCode::BAD_REQUEST, "nome curto: {body}");
            continue;
        }
        assert_eq!(status, StatusCode::CREATED, "{body}");
    }
    let (status, body) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Quatro" }))).await;
    assert_eq!(status, StatusCode::CREATED, "{body}");
    let (status, err) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Cinco" }))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "too_many_characters");

    // Apagar libera a vaga e o nome.
    let id = body["id"].as_str().unwrap();
    let (status, _) = api.call("DELETE", &format!("/v1/characters/{id}"), Some(&t), false, None).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (status, _) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Quatro" }))).await;
    assert_eq!(status, StatusCode::CREATED);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn entrar_no_servidor_e_salvar(db: PgPool) {
    let api = Api::new(db.clone());
    let a = api.register(&db, "Paulo").await;
    let b = api.register(&db, "Joao").await;
    let (_, ch) = api.call("POST", "/v1/characters", Some(&a), false, Some(json!({ "name": "Aranha" }))).await;
    let id = ch["id"].as_str().unwrap().to_string();
    assert_eq!(ch["active_weapon"], "Sword");
    assert!(ch["position"].is_null());

    // Só o dono pede passe.
    let (status, _) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&b), false, None).await;
    assert_eq!(status, StatusCode::NOT_FOUND);
    let (status, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&a), false, None).await;
    assert_eq!(status, StatusCode::OK);
    let ticket = join["ticket"].as_str().unwrap();

    // Servidor sem segredo não resgata.
    let (status, _) = api.call("POST", "/internal/tickets/redeem", None, false, Some(json!({ "ticket": ticket }))).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    let (status, got) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": ticket }))).await;
    assert_eq!(status, StatusCode::OK, "{got}");
    assert_eq!(got["account_name"], "Paulo");
    assert_eq!(got["character"]["name"], "Aranha");
    // Uso único.
    let (status, err) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": ticket }))).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    assert_eq!(err["error"], "invalid_ticket");

    let state = json!({
        "map": "L_SouthWindsward",
        "position": { "x": -934955.5, "y": 270224.5, "z": 7960.8 },
        "yaw": -94.0,
        "health": 812.5,
        "active_weapon": "Hatchet",
        "loadout": { "Sword": ["ShieldBash", "WholeheartedSlash", "FinalStand"] },
        "mastery": { "Sword": ["ShieldBashPassive1"] }
    });
    let (status, _) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state.clone())).await;
    assert_eq!(status, StatusCode::NO_CONTENT);

    let (_, me) = api.call("GET", "/v1/me", Some(&a), false, None).await;
    let saved = &me["characters"][0];
    assert_eq!(saved["position"], state["position"]);
    assert_eq!(saved["active_weapon"], "Hatchet");
    assert_eq!(saved["loadout"], state["loadout"]);
    assert!(saved["last_seen_at"].is_string());
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn convite_pela_api_so_admin(db: PgPool) {
    let api = Api::new(db.clone());
    let t = api.register(&db, "Paulo").await;
    let (status, _) = api.call("POST", "/v1/invites", Some(&t), false, None).await;
    assert_eq!(status, StatusCode::FORBIDDEN);
    sqlx::query("update accounts set is_admin = true where name = 'Paulo'").execute(&db).await.unwrap();
    let (status, body) = api.call("POST", "/v1/invites", Some(&t), false, None).await;
    assert_eq!(status, StatusCode::CREATED);
    assert_eq!(body["code"].as_str().unwrap().len(), 19);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn inventario_salva_e_volta(db: PgPool) {
    let api = Api::new(db.clone());
    let t = api.register(&db, "Paulo").await;
    let (_, ch) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Aranha" }))).await;
    let id = ch["id"].as_str().unwrap().to_string();

    async fn redeem(api: &Api, t: &str, id: &str) -> Value {
        let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(t), false, None).await;
        let (status, got) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": join["ticket"] }))).await;
        assert_eq!(status, StatusCode::OK, "{got}");
        got["character"].clone()
    }

    let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    let (_, first) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": join["ticket"] }))).await;
    assert_eq!(first["first_join"], true);
    assert_eq!(first["character"]["coins"], 0);
    assert_eq!(first["character"]["inventory"], json!([]));
    assert_eq!(first["character"]["progression"], json!({}));
    let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    let (_, second) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": join["ticket"] }))).await;
    assert_eq!(second["first_join"], false);

    let base = json!({ "map": "L_SouthWindsward", "position": null, "yaw": 0.0, "health": null, "active_weapon": "Sword",
                       "loadout": {}, "mastery": {} });
    let mut state = base.clone();
    state["coins"] = json!(120);
    state["progression"] = json!({ "level": 3, "xp": 1200, "spent": [0, 0, 0, 0, 5], "trade": [640, 0, 110, 0] });
    state["inventory"] = json!([
        { "item_id": "1hSwordT2", "quantity": 1, "container": "equip", "slot": 0, "gear_score": 250, "rarity": 1 },
        { "item_id": "OreT1", "quantity": 37, "container": "bag", "slot": 0 },
        { "item_id": "PotionHealthT1", "quantity": 5, "container": "bag", "slot": 1 }
    ]);
    let (status, body) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{body}");

    let c = redeem(&api, &t, &id).await;
    assert_eq!(c["coins"], 120);
    let inv = c["inventory"].as_array().unwrap();
    assert_eq!(inv.len(), 3);
    assert!(inv.iter().any(|i| i["item_id"] == "OreT1" && i["quantity"] == 37));
    assert!(inv.iter().any(|i| i["item_id"] == "1hSwordT2" && i["gear_score"] == 250 && i["container"] == "equip"));
    assert_eq!(c["progression"]["level"], 3);
    assert_eq!(c["progression"]["trade"], json!([640, 0, 110, 0]));

    // Sem inventário/moedas no salvamento: mantém.
    let (status, _) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(base.clone())).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let c = redeem(&api, &t, &id).await;
    assert_eq!(c["coins"], 120);
    assert_eq!(c["inventory"].as_array().unwrap().len(), 3);
    assert_eq!(c["progression"]["xp"], 1200);
    let mut s = base.clone();
    s["progression"] = json!([1, 2]);
    let (status, err) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(s)).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    assert_eq!(err["error"], "invalid_progression");

    // Slot repetido e quantidade zero são recusados, e nada muda.
    for bad in [
        json!([{ "item_id": "OreT1", "quantity": 1, "container": "bag", "slot": 0 }, { "item_id": "WoodT1", "quantity": 1, "container": "bag", "slot": 0 }]),
        json!([{ "item_id": "OreT1", "quantity": 0, "container": "bag", "slot": 0 }]),
        json!([{ "item_id": "OreT1", "quantity": 1, "container": "chest", "slot": 0 }]),
    ] {
        let mut s = base.clone();
        s["inventory"] = bad;
        let (status, err) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(s)).await;
        assert_eq!(status, StatusCode::BAD_REQUEST);
        assert_eq!(err["error"], "invalid_inventory");
    }
    let c = redeem(&api, &t, &id).await;
    assert_eq!(c["inventory"].as_array().unwrap().len(), 3);
}

/// Personagem novo; devolve o id.
async fn character(api: &Api, db: &PgPool, account: &str, name: &str) -> String {
    let t = api.register(db, account).await;
    let (_, ch) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": name }))).await;
    ch["id"].as_str().unwrap().to_string()
}

async fn coins_of(db: &PgPool, id: &str) -> i64 {
    let (c,): (i64,) = sqlx::query_as("select coins from characters where id = $1::uuid").bind(id).fetch_one(db).await.unwrap();
    c
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn armazem_grava_com_a_bolsa_e_confere_versao(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let url = format!("/internal/characters/{id}/storage/settlement:4");

    let (status, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(s, json!({ "version": 0, "items": [] }));

    // Deposita 30 de 37 minérios: bolsa e armazém na mesma chamada.
    let put = json!({ "version": 0, "coins": 50,
                      "items": [{ "item_id": "OreT1", "quantity": 30, "slot": 0 }],
                      "inventory": [{ "item_id": "OreT1", "quantity": 7, "container": "bag", "slot": 0 }] });
    let (status, s) = api.call("PUT", &url, None, true, Some(put.clone())).await;
    assert_eq!(status, StatusCode::OK, "{s}");
    assert_eq!(s["version"], 1);
    assert_eq!(coins_of(&db, &id).await, 50);

    // Versão velha: recusa e não mexe em nada.
    let mut stale = put.clone();
    stale["coins"] = json!(999);
    let (status, err) = api.call("PUT", &url, None, true, Some(stale)).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "storage_changed");
    assert_eq!(coins_of(&db, &id).await, 50);

    let (_, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(s["items"][0]["quantity"], 30);

    // Local inválido e armazém de casa que não é dele.
    let (status, _) = api.call("GET", &format!("/internal/characters/{id}/storage/cidade"), None, true, None).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    let mut house = put.clone();
    house["version"] = json!(0);
    let other = format!("/internal/characters/{id}/storage/house:{}", uuid::Uuid::new_v4());
    let (status, _) = api.call("PUT", &other, None, true, Some(house)).await;
    assert_eq!(status, StatusCode::NOT_FOUND);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn mercado_venda_compra_e_credito(db: PgPool) {
    let api = Api::new(db.clone());
    let seller = character(&api, &db, "Paulo", "Aranha").await;
    let buyer = character(&api, &db, "Joao", "Besouro").await;
    let empty = json!([]);

    // Vende 10 minérios a 5 cada (taxa 2): o item sai da bolsa e fica na ordem.
    let sell = json!({ "character_id": seller, "kind": "sell", "item_id": "OreT1", "quantity": 10, "unit_price": 5,
                       "fee": 2, "duration_hours": 24, "coins": 98, "inventory": empty });
    let (status, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(sell)).await;
    assert_eq!(status, StatusCode::CREATED, "{o}");
    let oid = o["id"].as_str().unwrap().to_string();

    let (_, list) = api.call("GET", "/internal/market/4/orders?item_id=OreT1", None, true, None).await;
    assert_eq!(list.as_array().unwrap().len(), 1);

    // Não compra da própria ordem nem mais do que há.
    let fill = |who: &str, q: i32, coins: i64| json!({ "character_id": who, "quantity": q, "coins": coins, "inventory": [] });
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&seller, 1, 0))).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&buyer, 11, 0))).await;
    assert_eq!(status, StatusCode::CONFLICT);

    // Compra 4: vão para o armazém da cidade do comprador; o vendedor ganha crédito de 20.
    let (status, o) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&buyer, 4, 80))).await;
    assert_eq!(status, StatusCode::OK, "{o}");
    assert_eq!(o["quantity"], 6);
    assert_eq!(coins_of(&db, &buyer).await, 80);
    let (_, s) = api.call("GET", &format!("/internal/characters/{buyer}/storage/settlement:4"), None, true, None).await;
    assert_eq!(s["items"][0]["item_id"], "OreT1");
    assert_eq!(s["items"][0]["quantity"], 4);
    assert_eq!(s["version"], 1);

    // Cancelar: só o dono; o resto (6) volta ao armazém do vendedor.
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/cancel"), None, true, Some(json!({ "character_id": buyer }))).await;
    assert_eq!(status, StatusCode::FORBIDDEN);
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/cancel"), None, true, Some(json!({ "character_id": seller }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (_, s) = api.call("GET", &format!("/internal/characters/{seller}/storage/settlement:4"), None, true, None).await;
    assert_eq!(s["items"][0]["quantity"], 6);
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&buyer, 1, 0))).await;
    assert_eq!(status, StatusCode::CONFLICT);

    // Crédito resgatado uma vez só.
    let (_, c) = api.call("POST", &format!("/internal/characters/{seller}/credits/claim"), None, true, None).await;
    assert_eq!(c["amount"], 20);
    let (_, c) = api.call("POST", &format!("/internal/characters/{seller}/credits/claim"), None, true, None).await;
    assert_eq!(c["amount"], 0);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn mercado_ordem_de_compra_e_vencimento(db: PgPool) {
    let api = Api::new(db.clone());
    let buyer = character(&api, &db, "Paulo", "Aranha").await;
    let seller = character(&api, &db, "Joao", "Besouro").await;

    // Ordem de compra de 3 a 10 (moedas em garantia): o vendedor entrega 2, que caem no armazém do comprador.
    let buy = json!({ "character_id": buyer, "kind": "buy", "item_id": "WoodT1", "quantity": 3, "unit_price": 10,
                      "duration_hours": 1, "coins": 0, "inventory": [] });
    let (_, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(buy)).await;
    let oid = o["id"].as_str().unwrap().to_string();
    let fill = json!({ "character_id": seller, "quantity": 2, "coins": 20, "inventory": [] });
    let (status, o) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill)).await;
    assert_eq!(status, StatusCode::OK, "{o}");
    let (_, s) = api.call("GET", &format!("/internal/characters/{buyer}/storage/settlement:4"), None, true, None).await;
    assert_eq!(s["items"][0]["quantity"], 2);

    // Vence: o que sobrou da garantia (1 × 10) volta em crédito.
    sqlx::query("update market_orders set expires_at = now() - interval '1 minute'").execute(&db).await.unwrap();
    let (_, list) = api.call("GET", "/internal/market/4/orders", None, true, None).await;
    assert_eq!(list, json!([]));
    let (_, c) = api.call("POST", &format!("/internal/characters/{buyer}/credits/claim"), None, true, None).await;
    assert_eq!(c["amount"], 10);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn casa_compra_e_armazem(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let buy = json!({ "territory": "4", "address": "cityd_house_address_001", "house_type": "Tier1", "price": 500000,
                      "coins": 1000, "inventory": [] });
    let (status, h) = api.call("POST", &format!("/internal/characters/{id}/houses"), None, true, Some(buy.clone())).await;
    assert_eq!(status, StatusCode::CREATED, "{h}");
    assert_eq!(coins_of(&db, &id).await, 1000);
    let (status, err) = api.call("POST", &format!("/internal/characters/{id}/houses"), None, true, Some(buy)).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "house_owned");

    let url = format!("/internal/characters/{id}/storage/house:{}", h["id"].as_str().unwrap());
    let put = json!({ "version": 0, "coins": 1000, "items": [{ "item_id": "OreT1", "quantity": 3, "slot": 0 }], "inventory": [] });
    let (status, s) = api.call("PUT", &url, None, true, Some(put)).await;
    assert_eq!(status, StatusCode::OK, "{s}");
    let (_, houses) = api.call("GET", &format!("/internal/characters/{id}/houses"), None, true, None).await;
    assert_eq!(houses[0]["address"], "cityd_house_address_001");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn brindes_de_npc_uma_vez_por_periodo(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let url = format!("/internal/characters/{id}/npc-rewards");
    let bag = json!([{ "item_id": "OreT1", "quantity": 10, "container": "bag", "slot": 0 }]);

    let starter = json!({ "npc": "0407a_Commander", "kind": "starter", "period": "once", "coins": 0, "inventory": bag });
    let (status, c) = api.call("POST", &url, None, true, Some(starter.clone())).await;
    assert_eq!(status, StatusCode::CREATED, "{c}");
    let (status, err) = api.call("POST", &url, None, true, Some(starter)).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "already_claimed");

    // Semanal: um por período; a semana seguinte é outro período.
    let weekly = |p: &str| json!({ "npc": "0407a_Commander", "kind": "weekly", "period": p, "coins": 5, "inventory": [] });
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("2026-10-13"))).await;
    assert_eq!(status, StatusCode::CREATED);
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("2026-10-13"))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("2026-10-20"))).await;
    assert_eq!(status, StatusCode::CREATED);
    assert_eq!(coins_of(&db, &id).await, 5);

    // Período errado para o tipo.
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("once"))).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);

    let (_, list) = api.call("GET", &format!("{url}?period=2026-10-13"), None, true, None).await;
    let kinds: Vec<_> = list["claims"].as_array().unwrap().iter().map(|c| (c["kind"].as_str().unwrap(), c["period"].as_str().unwrap())).collect();
    assert_eq!(kinds, vec![("starter", "once"), ("weekly", "2026-10-13")]);
}
