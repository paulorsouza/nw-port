//! Full flow against a real Postgres. `#[sqlx::test]` creates a temporary database per test from
//! `DATABASE_URL` (see README).

use std::sync::atomic::{AtomicI64, Ordering};
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
    db: PgPool,
    seq: AtomicI64,
}

impl Api {
    fn new(db: PgPool) -> Self {
        Self::with_maps(db, "")
    }

    /// With a game server per map (`NW_MAP_SERVERS`).
    fn with_maps(db: PgPool, maps: &str) -> Self {
        let map_servers = Arc::new(nw_core::maps::MapServers::parse(maps).unwrap());
        Self { router: app(AppState { db: db.clone(), server_secret: Arc::from(SECRET), map_servers }), db, seq: AtomicI64::new(0) }
    }

    /// Game server saves carry session and number (journal.rs): when the test does not send them, uses the character's
    /// current session and an increasing number, as the server does.
    async fn with_seq(&self, method: &str, uri: &str, server: bool, body: Option<Value>) -> Option<Value> {
        let write = ["/state", "/storage/", "/houses", "/npc-rewards", "/credits/claim", "/orders", "/fill", "/ground", "/take", "/city/buy", "/sell-raw", "/buy-item", "/city/bid"];
        if !server || method == "GET" || !write.iter().any(|w| uri.contains(w)) || uri.contains("/cancel") {
            return body;
        }
        let mut b = body.unwrap_or_else(|| json!({}));
        if b.get("seq").is_some() || !b.is_object() {
            return Some(b);
        }
        let id = uri
            .strip_prefix("/internal/characters/")
            .and_then(|r| r.split('/').next())
            .map(str::to_string)
            .or_else(|| b["character_id"].as_str().map(str::to_string));
        let Some(id) = id else { return Some(b) };
        let session: Option<(i64,)> = sqlx::query_as("select session from characters where id::text = $1").bind(&id).fetch_optional(&self.db).await.unwrap();
        b["session"] = json!(session.map_or(0, |s| s.0));
        b["seq"] = json!(self.seq.fetch_add(1, Ordering::SeqCst) + 1);
        Some(b)
    }

    async fn call(&self, method: &str, uri: &str, token: Option<&str>, server: bool, body: Option<Value>) -> (StatusCode, Value) {
        let body = self.with_seq(method, uri, server, body).await;
        self.send(method, uri, token, server, body.map(|b| b.to_string())).await
    }

    /// Body as text, as the game server sends it (numbers with 17 digits, for example).
    async fn send(&self, method: &str, uri: &str, token: Option<&str>, server: bool, body: Option<String>) -> (StatusCode, Value) {
        let mut req = Request::builder().method(method).uri(uri);
        if let Some(t) = token {
            req = req.header("authorization", format!("Bearer {t}"));
        }
        if server {
            req = req.header("x-nw-server-secret", SECRET);
        }
        let req = match body {
            Some(b) => req.header("content-type", "application/json").body(Body::from(b)),
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
            assert_eq!(status, StatusCode::BAD_REQUEST, "short name: {body}");
            continue;
        }
        assert_eq!(status, StatusCode::CREATED, "{body}");
    }
    let (status, body) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Quatro" }))).await;
    assert_eq!(status, StatusCode::CREATED, "{body}");
    let (status, err) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Cinco" }))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "too_many_characters");

    // Deleting frees the slot and the name.
    let id = body["id"].as_str().unwrap();
    let (status, _) = api.call("DELETE", &format!("/v1/characters/{id}"), Some(&t), false, None).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (status, _) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Quatro" }))).await;
    assert_eq!(status, StatusCode::CREATED);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn servidor_por_mapa(db: PgPool) {
    // Without NW_MAP_SERVERS the join has no address (the client uses -nwconnect) and travel is refused.
    let plain = Api::new(db.clone());
    let t = plain.register(&db, "Paulo").await;
    let (_, ch) = plain.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Aranha" }))).await;
    let id = ch["id"].as_str().unwrap().to_string();
    let (status, join) = plain.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    assert_eq!(status, StatusCode::OK);
    assert!(join.get("address").is_none(), "{join}");
    let (status, err) = plain.call("POST", &format!("/internal/characters/{id}/travel"), None, true, Some(json!({ "map": "L_Arena" }))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "map_not_served");

    let api = Api::with_maps(db.clone(), "L_SouthWindsward=10.0.0.2:7777,L_QuedaEterna=10.0.0.2:7778,L_Arena=10.0.0.2:7779");
    // New character: the start map's server.
    let (status, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(join["address"], "10.0.0.2:7777");
    assert_eq!(join["map"], "L_SouthWindsward");
    let got = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": join["ticket"] }))).await.1;
    let mut s = json!({ "map": "L_SouthWindsward", "position": { "x": 1.0, "y": 2.0, "z": 3.0 }, "yaw": 0.0, "health": 100.0,
        "active_weapon": "Sword", "loadout": {}, "mastery": {}, "session": got["session"], "seq": 1 });
    let (status, body) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(s.clone())).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{body}");

    // Travel: only a game server; saves the map without position and gives a new ticket and the address.
    let (status, _) = api.call("POST", &format!("/internal/characters/{id}/travel"), Some(&t), false, Some(json!({ "map": "L_QuedaEterna" }))).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    let (status, err) = api.call("POST", &format!("/internal/characters/{id}/travel"), None, true, Some(json!({ "map": "L_Nada" }))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "map_not_served");
    let (status, moved) = api.call("POST", &format!("/internal/characters/{id}/travel"), None, true, Some(json!({ "map": "/Game/NW/Maps/L_QuedaEterna" }))).await;
    assert_eq!(status, StatusCode::OK, "{moved}");
    assert_eq!(moved["address"], "10.0.0.2:7778");
    assert_eq!(moved["map"], "L_QuedaEterna");
    let (status, got) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": moved["ticket"] }))).await;
    assert_eq!(status, StatusCode::OK, "{got}");
    assert_eq!(got["character"]["map"], "L_QuedaEterna");
    assert!(got["character"]["position"].is_null(), "{got}");

    // The old server's late save belongs to the previous session: refused, the map stays.
    s["seq"] = json!(2);
    let (status, _) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(s)).await;
    assert_eq!(status, StatusCode::CONFLICT);
    let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    assert_eq!(join["address"], "10.0.0.2:7778");

    // A saved map without a server: enters through the start map.
    sqlx::query("update characters set map = 'L_Velha' where id::text = $1").bind(&id).execute(&db).await.unwrap();
    let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    assert_eq!(join["address"], "10.0.0.2:7777");
    assert_eq!(join["map"], "L_SouthWindsward");
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

    // Only the owner can request a ticket.
    let (status, _) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&b), false, None).await;
    assert_eq!(status, StatusCode::NOT_FOUND);
    let (status, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&a), false, None).await;
    assert_eq!(status, StatusCode::OK);
    let ticket = join["ticket"].as_str().unwrap();

    // A server without the secret cannot redeem.
    let (status, _) = api.call("POST", "/internal/tickets/redeem", None, false, Some(json!({ "ticket": ticket }))).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    let (status, got) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": ticket }))).await;
    assert_eq!(status, StatusCode::OK, "{got}");
    assert_eq!(got["account_name"], "Paulo");
    assert_eq!(got["is_admin"], false);
    assert_eq!(got["character"]["name"], "Aranha");
    // Single use.
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

    // No inventory/coins in the save: keeps them.
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

    // Repeated slot and zero quantity are rejected, and nothing changes.
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
    // Equipment slots 0..21 (client NWEquip::Count: quick slots 15-18, bags 19-21); 22 is out.
    let mut s = base.clone();
    s["inventory"] = json!([{ "item_id": "BagAT5", "quantity": 1, "container": "equip", "slot": 21 }]);
    let (status, body) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(s.clone())).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{body}");
    s["inventory"][0]["slot"] = json!(22);
    let (status, _) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(s)).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
}

/// New character; returns the id.
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
    let url = format!("/internal/characters/{id}/storage/shared");

    let (status, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(s, json!({ "version": 0, "items": [], "tab_names": [], "capacity": 0.0 }));

    // Deposits 30 of 37 ores: bag and Storage in the same call.
    let put = json!({ "version": 0, "coins": 50,
                      "items": [{ "item_id": "OreT1", "quantity": 30, "slot": 0 }],
                      "inventory": [{ "item_id": "OreT1", "quantity": 7, "container": "bag", "slot": 0 }] });
    let (status, s) = api.call("PUT", &url, None, true, Some(put.clone())).await;
    assert_eq!(status, StatusCode::OK, "{s}");
    assert_eq!(s["version"], 1);
    assert_eq!(coins_of(&db, &id).await, 50);

    // Stale version: rejects and touches nothing.
    let mut stale = put.clone();
    stale["coins"] = json!(999);
    let (status, err) = api.call("PUT", &url, None, true, Some(stale)).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(err["error"], "storage_changed");
    assert_eq!(coins_of(&db, &id).await, 50);

    let (_, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(s["items"][0]["quantity"], 30);

    // Only the shared Storage exists.
    for other in ["cidade".to_string(), "settlement:4".to_string(), format!("house:{}", uuid::Uuid::new_v4())] {
        let (status, _) = api.call("GET", &format!("/internal/characters/{id}/storage/{other}"), None, true, None).await;
        assert_eq!(status, StatusCode::BAD_REQUEST, "{other}");
    }
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn armazem_abas_peso_e_capacidade(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let url = format!("/internal/characters/{id}/storage/shared");
    let put = json!({ "version": 0, "coins": 0, "inventory": [], "capacity": 2000.0, "tab_names": ["Minério", "", "", "", "Armas"],
                      "items": [{ "item_id": "OreT1", "quantity": 30, "slot": 0, "tab": 0, "weight": 0.1 },
                                { "item_id": "WoodT1", "quantity": 10, "slot": 1, "tab": 4, "weight": 0.2 }] });
    let (status, s) = api.call("PUT", &url, None, true, Some(put)).await;
    assert_eq!(status, StatusCode::OK, "{s}");
    let (_, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(s["capacity"], 2000.0);
    assert_eq!(s["tab_names"][4], "Armas");
    assert_eq!(s["items"][1]["tab"], 4);

    // Tab outside 0–4, negative weight and long name: rejected.
    for bad in [json!({ "item_id": "OreT1", "quantity": 1, "slot": 0, "tab": 5 }), json!({ "item_id": "OreT1", "quantity": 1, "slot": 0, "weight": -1.0 })] {
        let (status, _) = api.call("PUT", &url, None, true, Some(json!({ "version": 1, "coins": 0, "inventory": [], "items": [bad] }))).await;
        assert_eq!(status, StatusCode::BAD_REQUEST);
    }
    let long = json!({ "version": 1, "coins": 0, "inventory": [], "items": [], "tab_names": ["x".repeat(25)] });
    let (status, _) = api.call("PUT", &url, None, true, Some(long)).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    // The character save also updates the capacity (new city without opening Storage).
    let st = json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword", "loadout": {}, "mastery": {}, "storage_capacity": 3000.0 });
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(st)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
    let (_, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(s["capacity"], 3000.0);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn mercado_compra_respeita_o_armazem(db: PgPool) {
    let api = Api::new(db.clone());
    let buyer = character(&api, &db, "Paulo", "Aranha").await;
    let seller = character(&api, &db, "Joao", "Besouro").await;
    // 10 kg Storage with 6 kg used.
    let url = format!("/internal/characters/{buyer}/storage/shared");
    let put = json!({ "version": 0, "coins": 1000, "inventory": [], "capacity": 10.0,
                      "items": [{ "item_id": "StoneT1", "quantity": 6, "slot": 0, "weight": 1.0 }] });
    let (status, s) = api.call("PUT", &url, None, true, Some(put)).await;
    assert_eq!(status, StatusCode::OK, "{s}");
    let buy = |q: i32| json!({ "character_id": buyer, "kind": "buy", "item_id": "WoodT1", "quantity": q, "unit_price": 10,
                               "duration_hours": 1, "weight": 1.0, "coins": 900, "inventory": [] });

    // 3 kg fit; then 2 kg more do not (6 + 3 open + 2 > 10).
    let (status, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(buy(3))).await;
    assert_eq!(status, StatusCode::CREATED, "{o}");
    let (status, e) = api.call("POST", "/internal/market/4/orders", None, true, Some(buy(2))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(e["error"], "storage_full");

    // Selling has no limit, and an expired sell order returns to Storage even over capacity.
    let sell = json!({ "character_id": buyer, "kind": "sell", "item_id": "OreT1", "quantity": 50, "unit_price": 5,
                       "duration_hours": 1, "weight": 1.0, "coins": 900, "inventory": [] });
    let (status, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(sell)).await;
    assert_eq!(status, StatusCode::CREATED, "{o}");
    sqlx::query("update market_orders set expires_at = now() - interval '1 minute' where kind = 'sell'").execute(&db).await.unwrap();
    api.call("GET", "/internal/market/4/orders", None, true, None).await;
    let (_, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(s["items"].as_array().unwrap().len(), 2, "{s}");

    // Over the limit: buying from a sell order is also rejected.
    let other = json!({ "character_id": seller, "kind": "sell", "item_id": "FiberT1", "quantity": 1, "unit_price": 5,
                        "duration_hours": 1, "weight": 0.1, "coins": 0, "inventory": [] });
    let (_, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(other)).await;
    let oid = o["id"].as_str().unwrap().to_string();
    let fill = json!({ "character_id": buyer, "quantity": 1, "coins": 895, "inventory": [] });
    let (status, e) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill)).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(e["error"], "storage_full");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn mercado_venda_compra_e_credito(db: PgPool) {
    let api = Api::new(db.clone());
    let seller = character(&api, &db, "Paulo", "Aranha").await;
    let buyer = character(&api, &db, "Joao", "Besouro").await;
    let empty = json!([]);

    // Sells 10 ores at 5 each (fee 2): the item leaves the bag and stays in the order.
    let sell = json!({ "character_id": seller, "kind": "sell", "item_id": "OreT1", "quantity": 10, "unit_price": 5,
                       "fee": 2, "duration_hours": 24, "coins": 98, "inventory": empty });
    let (status, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(sell)).await;
    assert_eq!(status, StatusCode::CREATED, "{o}");
    let oid = o["id"].as_str().unwrap().to_string();

    let (_, list) = api.call("GET", "/internal/market/4/orders?item_id=OreT1", None, true, None).await;
    assert_eq!(list.as_array().unwrap().len(), 1);
    assert_eq!(list[0]["seller"], "Aranha");

    // Cannot buy from one's own order nor more than is available.
    let fill = |who: &str, q: i32, coins: i64| json!({ "character_id": who, "quantity": q, "coins": coins, "inventory": [] });
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&seller, 1, 0))).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&buyer, 11, 0))).await;
    assert_eq!(status, StatusCode::CONFLICT);

    // Buys 4: they go to the buyer's city Storage; the seller gets a credit of 20.
    let (status, o) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&buyer, 4, 80))).await;
    assert_eq!(status, StatusCode::OK, "{o}");
    assert_eq!(o["quantity"], 6);
    assert_eq!(coins_of(&db, &buyer).await, 80);
    let (_, s) = api.call("GET", &format!("/internal/characters/{buyer}/storage/shared"), None, true, None).await;
    assert_eq!(s["items"][0]["item_id"], "OreT1");
    assert_eq!(s["items"][0]["quantity"], 4);
    assert_eq!(s["version"], 1);

    // Cancel: only the owner; the rest (6) returns to the seller's Storage.
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/cancel"), None, true, Some(json!({ "character_id": buyer }))).await;
    assert_eq!(status, StatusCode::FORBIDDEN);
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/cancel"), None, true, Some(json!({ "character_id": seller }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (_, s) = api.call("GET", &format!("/internal/characters/{seller}/storage/shared"), None, true, None).await;
    assert_eq!(s["items"][0]["quantity"], 6);
    let (status, _) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill(&buyer, 1, 0))).await;
    assert_eq!(status, StatusCode::CONFLICT);

    // Credit claimed only once.
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

    // Buy order of 3 at 10 (coins held in escrow): the seller delivers 2, which land in the buyer's Storage.
    let buy = json!({ "character_id": buyer, "kind": "buy", "item_id": "WoodT1", "quantity": 3, "unit_price": 10,
                      "duration_hours": 1, "coins": 0, "inventory": [] });
    let (_, o) = api.call("POST", "/internal/market/4/orders", None, true, Some(buy)).await;
    let oid = o["id"].as_str().unwrap().to_string();
    let fill = json!({ "character_id": seller, "quantity": 2, "coins": 20, "inventory": [] });
    let (status, o) = api.call("POST", &format!("/internal/market/orders/{oid}/fill"), None, true, Some(fill)).await;
    assert_eq!(status, StatusCode::OK, "{o}");
    let (_, s) = api.call("GET", &format!("/internal/characters/{buyer}/storage/shared"), None, true, None).await;
    assert_eq!(s["items"][0]["quantity"], 2);

    // Expires: what is left of the escrow (1 × 10) returns as credit.
    sqlx::query("update market_orders set expires_at = now() - interval '1 minute'").execute(&db).await.unwrap();
    let (_, list) = api.call("GET", "/internal/market/4/orders", None, true, None).await;
    assert_eq!(list, json!([]));
    let (_, c) = api.call("POST", &format!("/internal/characters/{buyer}/credits/claim"), None, true, None).await;
    assert_eq!(c["amount"], 10);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn casa_compra(db: PgPool) {
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

    // Weekly: one per period; the next week is another period.
    let weekly = |p: &str| json!({ "npc": "0407a_Commander", "kind": "weekly", "period": p, "coins": 5, "inventory": [] });
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("2026-10-13"))).await;
    assert_eq!(status, StatusCode::CREATED);
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("2026-10-13"))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("2026-10-20"))).await;
    assert_eq!(status, StatusCode::CREATED);
    assert_eq!(coins_of(&db, &id).await, 5);

    // Wrong period for the kind.
    let (status, _) = api.call("POST", &url, None, true, Some(weekly("once"))).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);

    let (_, list) = api.call("GET", &format!("{url}?period=2026-10-13"), None, true, None).await;
    let kinds: Vec<_> = list["claims"].as_array().unwrap().iter().map(|c| (c["kind"].as_str().unwrap(), c["period"].as_str().unwrap())).collect();
    assert_eq!(kinds, vec![("starter", "once"), ("weekly", "2026-10-13")]);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn item_com_props(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let url = format!("/internal/characters/{id}/storage/shared");
    let props = json!({ "attr": 4, "quality": 87.5, "perks": ["ChanceCritica"], "bind": "equip", "signed": "Aranha" });

    // Storage with props: comes back the same.
    let put = json!({ "version": 0, "coins": 0, "inventory": [],
                      "items": [{ "item_id": "1hSwordT5", "quantity": 1, "slot": 0, "gear_score": 512, "props": props }] });
    let (status, s) = api.call("PUT", &url, None, true, Some(put)).await;
    assert_eq!(status, StatusCode::OK, "{s}");
    let (_, s) = api.call("GET", &url, None, true, None).await;
    assert_eq!(s["items"][0]["props"], props);

    // Bag with props: stored the same.
    let base = json!({ "map": "L_QuedaEterna", "position": { "x": 0.0, "y": 0.0, "z": 0.0 }, "yaw": 0.0, "health": 100.0,
                       "active_weapon": "Sword", "loadout": {}, "mastery": {} });
    let mut state = base.clone();
    state["inventory"] = json!([{ "item_id": "1hSwordT5", "quantity": 1, "container": "equip", "slot": 0, "props": props }]);
    let (status, _) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state)).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (props_db,): (Value,) = sqlx::query_as("select props from items where character_id = $1::uuid").bind(&id).fetch_one(&db).await.unwrap();
    assert_eq!(props_db, props);

    // props that is not an object: rejected.
    let mut bad = base.clone();
    bad["inventory"] = json!([{ "item_id": "OreT1", "quantity": 1, "container": "bag", "slot": 0, "props": [1] }]);
    let (status, _) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(bad)).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);

    // A bound item cannot go to the Trading Post.
    let sell = json!({ "character_id": id, "kind": "sell", "item_id": "1hSwordT5", "quantity": 1, "unit_price": 5,
                       "duration_hours": 24, "props": { "bound": true }, "coins": 0, "inventory": [] });
    let (status, err) = api.call("POST", "/internal/market/4/orders", None, true, Some(sell)).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    assert_eq!(err["error"], "item_bound");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn item_assinado_nao_duplica(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let other = character(&api, &db, "Joao", "Besouro").await;
    let (status, s) = api.call("POST", &format!("/internal/characters/{id}/serials"), None, true, None).await;
    assert_eq!(status, StatusCode::CREATED, "{s}");
    let serial = s["serial"].as_str().unwrap().to_string();
    let (_, s2) = api.call("POST", &format!("/internal/characters/{id}/serials"), None, true, None).await;
    assert_ne!(s2["serial"], s["serial"]);
    let sword = |slot: i32| json!({ "item_id": "1hSwordT5", "quantity": 1, "container": "bag", "slot": slot,
                                    "props": { "serial": serial, "quality": 91.0 } });
    let base = json!({ "map": "L_QuedaEterna", "position": { "x": 0.0, "y": 0.0, "z": 0.0 }, "yaw": 0.0, "health": 100.0,
                       "active_weapon": "Sword", "loadout": {}, "mastery": {} });
    let state = |items: Value| { let mut b = base.clone(); b["inventory"] = items; b };

    // In the bag: ok.
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state(json!([sword(0)])))).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
    // Twice in the same bag: rejected.
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state(json!([sword(0), sword(1)])))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(e["error"], "item_duplicate");
    // Made-up number: rejected.
    let fake = json!([{ "item_id": "1hSwordT5", "quantity": 1, "container": "bag", "slot": 0, "props": { "serial": "x-1" } }]);
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state(fake))).await;
    assert_eq!(status, StatusCode::BAD_REQUEST);
    assert_eq!(e["error"], "unknown_serial");
    // Another character with the same item while it is in the first one's bag: rejected.
    let (status, e) = api.call("PUT", &format!("/internal/characters/{other}/state"), None, true, Some(state(json!([sword(0)])))).await;
    assert_eq!(status, StatusCode::CONFLICT, "{e}");

    // Store: leaves the bag and enters Storage in the same save: ok; in Storage and in the bag at once: rejected.
    let url = format!("/internal/characters/{id}/storage/shared");
    let stored = json!([{ "item_id": "1hSwordT5", "quantity": 1, "slot": 0, "props": { "serial": serial, "quality": 91.0 } }]);
    let (status, e) = api.call("PUT", &url, None, true, Some(json!({ "version": 0, "coins": 0, "inventory": [], "items": stored }))).await;
    assert_eq!(status, StatusCode::OK, "{e}");
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state(json!([sword(0)])))).await;
    assert_eq!(status, StatusCode::CONFLICT);
    assert_eq!(e["error"], "item_duplicate");
    let (loc,): (String,) = sqlx::query_as("select location from item_serials where serial = $1").bind(&serial).fetch_one(&db).await.unwrap();
    assert_eq!(loc, "shared");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn gravacao_fora_de_ordem_e_sessao_velha(db: PgPool) {
    let api = Api::new(db.clone());
    let t = api.register(&db, "Paulo").await;
    let (_, ch) = api.call("POST", "/v1/characters", Some(&t), false, Some(json!({ "name": "Aranha" }))).await;
    let id = ch["id"].as_str().unwrap().to_string();
    let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    let (_, got) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": join["ticket"] }))).await;
    let session = got["session"].as_i64().unwrap();
    assert_eq!(session, 1);
    let url = format!("/internal/characters/{id}/state");
    let state = |seq: i64, session: i64, coins: i64, ore: i32| json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword",
        "loadout": {}, "mastery": {}, "session": session, "seq": seq, "reason": "teste", "coins": coins,
        "inventory": [{ "item_id": "OreT1", "quantity": ore, "container": "bag", "slot": 0 }] });

    let (status, e) = api.call("PUT", &url, None, true, Some(state(2, session, 50, 10))).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
    // Save 1 arrives after save 2: rejected; the same save 2 again: already applied.
    let (status, e) = api.call("PUT", &url, None, true, Some(state(1, session, 0, 99))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("stale_write")));
    let (status, e) = api.call("PUT", &url, None, true, Some(state(2, session, 0, 99))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("duplicate_write")));
    // No number: rejected.
    let (status, e) = api.call("PUT", &url, None, false, None).await;
    assert_ne!(status, StatusCode::NO_CONTENT, "{e}");

    // Joined again (another server): the late save from the previous session does not pass, even with a high number.
    let (_, join) = api.call("POST", &format!("/v1/characters/{id}/join"), Some(&t), false, None).await;
    let (_, got) = api.call("POST", "/internal/tickets/redeem", None, true, Some(json!({ "ticket": join["ticket"] }))).await;
    assert_eq!(got["character"]["coins"], 50);
    assert_eq!(got["character"]["inventory"][0]["quantity"], 10);
    let (status, e) = api.call("PUT", &url, None, true, Some(state(100, session, 0, 99))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("stale_write")));
    let (status, e) = api.call("PUT", &url, None, true, Some(state(1, session + 1, 60, 12))).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn log_de_itens(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let state = |coins: i64, items: Value| json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword",
        "loadout": {}, "mastery": {}, "reason": "periodic", "coins": coins, "inventory": items });
    let ore = |q: i32| json!([{ "item_id": "OreT1", "quantity": q, "container": "bag", "slot": 0 }]);
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state(10, ore(40)))).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
    // Stores 30 in tab 2.
    let put = json!({ "version": 0, "coins": 10, "reason": "storage", "inventory": ore(10),
                      "items": [{ "item_id": "OreT1", "quantity": 30, "slot": 0, "tab": 2, "weight": 0.1 }] });
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/storage/shared"), None, true, Some(put)).await;
    assert_eq!(status, StatusCode::OK, "{e}");

    let (_, log) = api.call("GET", &format!("/internal/item-log?character_id={id}"), None, true, None).await;
    let rows: Vec<(String, String, i64)> = log.as_array().unwrap().iter()
        .map(|r| (r["reason"].as_str().unwrap().to_string(), r["place"].as_str().unwrap().to_string(), r["quantity"].as_i64().unwrap()))
        .collect();
    assert!(rows.contains(&("periodic".into(), "bag".into(), 40)), "{rows:?}");
    assert!(rows.contains(&("periodic".into(), "coins".into(), 10)), "{rows:?}");
    assert!(rows.contains(&("storage".into(), "bag".into(), -30)), "{rows:?}");
    assert!(rows.contains(&("storage".into(), "storage:2".into(), 30)), "{rows:?}");
    assert_eq!(rows.len(), 4, "{rows:?}");
    let (_, by_item) = api.call("GET", "/internal/item-log?item_id=WoodT1", None, true, None).await;
    assert_eq!(by_item, json!([]));

    // Item with a decimal (game float quality, printed with 17 digits): saving the same twice adds no row.
    let sword = json!([{ "item_id": "1hSwordT5", "quantity": 1, "container": "bag", "slot": 0, "props": { "quality": 0.5, "attr": 4 } }]);
    let url = format!("/internal/characters/{id}/state");
    for _ in 0..2 {
        let body = api.with_seq("PUT", &url, true, Some(state(10, sword.clone()))).await.unwrap().to_string().replace("0.5", "99.300003051757812");
        let (status, e) = api.send("PUT", &url, None, true, Some(body)).await;
        assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
    }
    let (_, log) = api.call("GET", "/internal/item-log?item_id=1hSwordT5", None, true, None).await;
    assert_eq!(log.as_array().unwrap().len(), 1, "{log}");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn chao_soltar_pegar_e_vencer(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let ore = |q: i32| json!([{ "item_id": "OreT1", "quantity": q, "container": "bag", "slot": 0 }]);
    let stack = |q: i32| json!([{ "item_id": "OreT1", "quantity": q, "slot": 0, "weight": 0.1 }]);
    let st = json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword", "loadout": {}, "mastery": {}, "coins": 0, "inventory": ore(40) });
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(st)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");

    // Drops 10: the bag keeps 30 and the ground bag gets 10, in one save.
    let drop = json!({ "map": "L_QuedaEterna", "position": { "x": 1.0, "y": 2.0, "z": 3.0 }, "ttl_seconds": 180,
                       "items": stack(10), "inventory": ore(30), "coins": 0, "reason": "drop" });
    let (status, bag) = api.call("POST", &format!("/internal/characters/{id}/ground"), None, true, Some(drop)).await;
    assert_eq!(status, StatusCode::CREATED, "{bag}");
    let bag_id = bag["id"].as_str().unwrap().to_string();
    let (_, list) = api.call("GET", &format!("/internal/characters/{id}/ground"), None, true, None).await;
    assert_eq!(list[0]["items"][0]["quantity"], 10);
    assert_eq!(list[0]["position"]["z"], 3.0);

    // Takes 4 (6 left), then the rest: the ground bag disappears; taking again gives ground_gone.
    let take = |left: Value, bag_ore: i32| json!({ "character_id": id, "items": left, "inventory": ore(bag_ore), "coins": 0, "reason": "ground_take" });
    let (status, e) = api.call("POST", &format!("/internal/ground/{bag_id}/take"), None, true, Some(take(stack(6), 34))).await;
    assert_eq!(status, StatusCode::OK, "{e}");
    let (status, e) = api.call("POST", &format!("/internal/ground/{bag_id}/take"), None, true, Some(take(json!([]), 40))).await;
    assert_eq!(status, StatusCode::OK, "{e}");
    let (status, e) = api.call("POST", &format!("/internal/ground/{bag_id}/take"), None, true, Some(take(json!([]), 50))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("ground_gone")));
    let (_, list) = api.call("GET", &format!("/internal/characters/{id}/ground"), None, true, None).await;
    assert_eq!(list, json!([]));

    // Expired ground bag: disappears from the list and stays in the log as ground_expired.
    let drop = json!({ "map": "L_QuedaEterna", "position": { "x": 0.0, "y": 0.0, "z": 0.0 }, "ttl_seconds": 180,
                       "items": stack(5), "inventory": ore(35), "coins": 0, "reason": "drop" });
    let (_, bag) = api.call("POST", &format!("/internal/characters/{id}/ground"), None, true, Some(drop)).await;
    sqlx::query("update ground_bags set expires_at = now() - interval '1 second'").execute(&db).await.unwrap();
    let (_, list) = api.call("GET", &format!("/internal/characters/{id}/ground"), None, true, None).await;
    assert_eq!(list, json!([]));
    let (_, log) = api.call("GET", &format!("/internal/item-log?character_id={id}"), None, true, None).await;
    let place = format!("ground:{}", bag["id"].as_str().unwrap());
    assert!(log.as_array().unwrap().iter().any(|r| r["reason"] == "ground_expired" && r["place"] == place.as_str() && r["quantity"] == -5), "{log}");
    assert!(log.as_array().unwrap().iter().any(|r| r["reason"] == "drop" && r["place"] == "bag" && r["quantity"] == -10), "{log}");
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn chao_item_assinado_nao_duplica(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let (_, s) = api.call("POST", &format!("/internal/characters/{id}/serials"), None, true, None).await;
    let serial = s["serial"].as_str().unwrap().to_string();
    let sword = json!({ "item_id": "1hSwordT5", "quantity": 1, "slot": 0, "props": { "serial": serial } });
    let drop = json!({ "map": "L_QuedaEterna", "position": { "x": 0.0, "y": 0.0, "z": 0.0 }, "ttl_seconds": 180,
                       "items": [sword.clone()], "inventory": [], "coins": 0, "reason": "drop" });
    let (status, e) = api.call("POST", &format!("/internal/characters/{id}/ground"), None, true, Some(drop)).await;
    assert_eq!(status, StatusCode::CREATED, "{e}");
    // On the ground and in the bag at once: rejected.
    let mut in_bag = sword.clone();
    in_bag["container"] = json!("bag");
    let st = json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword", "loadout": {}, "mastery": {}, "inventory": [in_bag] });
    let (status, e) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(st)).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("item_duplicate")));
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn bolsa_por_diferenca(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    let url = format!("/internal/characters/{id}/state");
    let base = || json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword", "loadout": {}, "mastery": {} });
    let mut st = base();
    st["inventory"] = json!([{ "item_id": "OreT1", "quantity": 10, "container": "bag", "slot": 0 },
                             { "item_id": "WoodT1", "quantity": 5, "container": "bag", "slot": 1 }]);
    st["reason"] = json!("periodic");
    let (status, e) = api.call("PUT", &url, None, true, Some(st)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");

    // Gathering: only slot 0 changes (10 → 13) and fiber goes into slot 2; slot 1 is cleared.
    let mut st = base();
    st["reason"] = json!("gather");
    st["inventory_patch"] = json!([
        { "container": "bag", "slot": 0, "item": { "item_id": "OreT1", "quantity": 13 } },
        { "container": "bag", "slot": 1, "item": null },
        { "container": "bag", "slot": 2, "item": { "item_id": "FiberT1", "quantity": 4 } }]);
    let (status, e) = api.call("PUT", &url, None, true, Some(st)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{e}");
    let rows: Vec<(String, i32, i32)> = sqlx::query_as("select item_id, quantity, slot from items where character_id = $1::uuid order by slot")
        .bind(&id).fetch_all(&db).await.unwrap();
    assert_eq!(rows, vec![("OreT1".into(), 13, 0), ("FiberT1".into(), 4, 2)]);
    let (_, log) = api.call("GET", &format!("/internal/item-log?character_id={id}"), None, true, None).await;
    let gather: Vec<(String, i64)> = log.as_array().unwrap().iter().filter(|r| r["reason"] == "gather")
        .map(|r| (r["item_id"].as_str().unwrap().to_string(), r["quantity"].as_i64().unwrap())).collect();
    assert_eq!(gather.len(), 3, "{gather:?}");
    assert!(gather.contains(&("OreT1".into(), 3)) && gather.contains(&("WoodT1".into(), -5)) && gather.contains(&("FiberT1".into(), 4)), "{gather:?}");

    // Repeated slot or wrong container: rejected without changing anything.
    for bad in [json!([{ "container": "bag", "slot": 0, "item": null }, { "container": "bag", "slot": 0, "item": null }]),
                json!([{ "container": "chest", "slot": 0, "item": null }])] {
        let mut st = base();
        st["inventory_patch"] = bad;
        let (status, _) = api.call("PUT", &url, None, true, Some(st)).await;
        assert_eq!(status, StatusCode::BAD_REQUEST);
    }

    // Signed item in two slots via the patch: rejected.
    let (_, s) = api.call("POST", &format!("/internal/characters/{id}/serials"), None, true, None).await;
    let serial = s["serial"].as_str().unwrap().to_string();
    let sword = json!({ "item_id": "1hSwordT5", "quantity": 1, "props": { "serial": serial } });
    let mut st = base();
    st["inventory_patch"] = json!([{ "container": "bag", "slot": 5, "item": sword }, { "container": "bag", "slot": 6, "item": sword }]);
    let (status, e) = api.call("PUT", &url, None, true, Some(st)).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("item_duplicate")));
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn creditos_ate_e_limite_de_ordens(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    for amount in [100_i64, 250] {
        sqlx::query("insert into coin_credits (id, character_id, amount, reason) values (gen_random_uuid(), $1::uuid, $2, 'market_sale')")
            .bind(&id).bind(amount).execute(&db).await.unwrap();
    }
    let (_, p) = api.call("GET", &format!("/internal/characters/{id}/credits"), None, true, None).await;
    assert_eq!(p["amount"], 350);
    // A new credit after the query is not included in this claim.
    tokio::time::sleep(std::time::Duration::from_millis(20)).await;
    sqlx::query("insert into coin_credits (id, character_id, amount, reason) values (gen_random_uuid(), $1::uuid, 7, 'market_sale')")
        .bind(&id).execute(&db).await.unwrap();
    let (status, c) = api.call("POST", &format!("/internal/characters/{id}/credits/claim"), None, true,
        Some(json!({ "until": p["until"], "reason": "market_credit" }))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!(c["amount"], 350);
    assert_eq!(coins_of(&db, &id).await, 350);
    let (_, p) = api.call("GET", &format!("/internal/characters/{id}/credits"), None, true, None).await;
    assert_eq!(p["amount"], 7);

    // 50 open orders: the 51st is rejected.
    sqlx::query("insert into market_orders (id, territory, kind, character_id, item_id, quantity, unit_price, expires_at)
                 select gen_random_uuid(), 'global', 'buy', $1::uuid, 'OreT1', 1, 1, now() + interval '1 day' from generate_series(1, 50)")
        .bind(&id).execute(&db).await.unwrap();
    let buy = json!({ "character_id": id, "kind": "buy", "item_id": "WoodT1", "quantity": 1, "unit_price": 1,
                      "duration_hours": 24, "coins": 0, "inventory": [] });
    let (status, e) = api.call("POST", "/internal/market/global/orders", None, true, Some(buy)).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("too_many_orders")));
}

// ---------------------------------------------------------------- cities (migration 0013)

const CITY: &str = "/internal/regions/1/city";

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn cidade_compra_precos_limites_e_projetos(db: PgPool) {
    let api = Api::new(db.clone());
    let gov = character(&api, &db, "Paulo", "Aranha").await;
    let player = character(&api, &db, "Joao", "Besouro").await;

    let (status, c) = api.call("GET", CITY, None, true, None).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert!(c["owner_id"].is_null());
    assert_eq!(c["treasury"], 20_000_000);
    let price = c["price"].as_i64().unwrap();

    // Buys at the fixed price; another price = 409.
    let (status, e) = api.call("POST", &format!("{CITY}/buy"), None, true, Some(json!({ "character_id": gov, "price": 1, "coins": 0, "inventory": [] }))).await;
    assert_eq!(status, StatusCode::CONFLICT, "{e}");
    assert_eq!(e["error"], "price_changed");
    let (status, c) = api.call("POST", &format!("{CITY}/buy"), None, true, Some(json!({ "character_id": gov, "price": price, "coins": 0, "inventory": [] }))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!(c["owner_name"], "Aranha");

    // Player sells iron ore at the default (middle of the range): paid from the treasury, added to the stock.
    let ore = c["buy"].as_array().unwrap().iter().find(|r| r["item_id"] == "OreT1").unwrap().clone();
    let p = ore["padrao"].as_i64().unwrap();
    assert_eq!((ore["min"].as_i64().unwrap(), ore["max"].as_i64().unwrap()), (20, 100));
    let sell = |who: &str, qty: i64, unit: i64, coins: i64| json!({ "character_id": who, "item_id": "OreT1", "quantity": qty, "unit_price": unit, "coins": coins, "inventory": [] });
    let (status, c) = api.call("POST", &format!("{CITY}/sell-raw"), None, true, Some(sell(&player, 100, p, 100 * p))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!(c["treasury"], 20_000_000 - 100 * p);
    assert_eq!(c["stock"]["OreT1"], 100);
    assert_eq!(coins_of(&db, &player).await, 100 * p);
    // Limit of 5 thousand per week per character and raw material.
    assert_eq!(ore["limit"], 5_000);
    let (status, e) = api.call("POST", &format!("{CITY}/sell-raw"), None, true, Some(sell(&player, 4_901, p, 0))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("city_limit")));

    // Only the governor changes prices, and within the range.
    let (status, e) = api.call("PUT", CITY, None, true, Some(json!({ "character_id": player, "tax": 0.02 }))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("not_governor")));
    let (status, e) = api.call("PUT", CITY, None, true, Some(json!({ "character_id": gov, "buy_prices": { "OreT1": 101 } }))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::BAD_REQUEST, Some("price_out_of_range")));
    let (status, c) = api.call("PUT", CITY, None, true, Some(json!({ "character_id": gov, "buy_prices": { "OreT1": 100 }, "tax": 0.08 }))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert!((c["tax"].as_f64().unwrap() - 0.08).abs() < 1e-6);
    // Stale price = 409; the governor selling to his own city gets at most the default.
    let (status, e) = api.call("POST", &format!("{CITY}/sell-raw"), None, true, Some(sell(&player, 10, p, 0))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("price_changed")));
    let (status, _) = api.call("POST", &format!("{CITY}/sell-raw"), None, true, Some(sell(&player, 10, 100, 0))).await;
    assert_eq!(status, StatusCode::OK);
    let (status, e) = api.call("POST", &format!("{CITY}/sell-raw"), None, true, Some(sell(&gov, 10, 100, 0))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("price_changed")));
    let (status, _) = api.call("POST", &format!("{CITY}/sell-raw"), None, true, Some(sell(&gov, 10, p, 0))).await;
    assert_eq!(status, StatusCode::OK);

    // List: potion at the default, goes into the treasury.
    let (_, before) = api.call("GET", CITY, None, true, None).await;
    let pot = before["sell"].as_array().unwrap().iter().find(|r| r["item_id"] == "PotionHealthT5").unwrap()["price"].as_i64().unwrap();
    let buy = json!({ "character_id": player, "item_id": "PotionHealthT5", "quantity": 10, "unit_price": pot, "coins": 0,
                      "inventory": [{ "item_id": "PotionHealthT5", "quantity": 10, "container": "bag", "slot": 0 }] });
    let (status, c) = api.call("POST", &format!("{CITY}/buy-item"), None, true, Some(buy)).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!(c["treasury"].as_i64().unwrap(), before["treasury"].as_i64().unwrap() + 10 * pot);
    assert_eq!(c["this_week"]["list_units"], 10);

    // Projects: station and weapon take a material basket from stock + coins and 24 h of work; a buff is paid from the treasury.
    let proj = |kind: &str, station: Option<&str>, other: Option<&str>| json!({ "character_id": gov, "kind": kind, "station": station,
                                                                              "buff": other, "weapon": other });
    let (status, e) = api.call("POST", &format!("{CITY}/projects"), None, true, Some(proj("station", Some("smelting"), None))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("city_no_stock")));
    let st = c["stations"].as_array().unwrap().iter().find(|s| s["station"] == "smelting").unwrap().clone();
    assert_eq!(st["tier"], 2);
    assert_eq!((c["weapons"].as_array().unwrap().len(), c["defense"].as_i64()), (5, Some(5)));
    let (status, c) = api.call("POST", &format!("{CITY}/projects"), None, true, Some(proj("buff", Some("smelting"), Some("xp")))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!(c["buffs"], json!([["smelting", "xp"]]));
    let (status, e) = api.call("POST", &format!("{CITY}/projects"), None, true, Some(proj("buff", Some("smelting"), Some("xp")))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("buff_active")));
    // With the tier 3 basket in stock, the smelter goes into work; a second work on the same station = 409.
    for item in ["OreT1", "WoodT2", "FiberT1", "RawhideT1", "StoneT1"] {
        sqlx::query("insert into city_stock (region, item_id, quantity) values ('1', $1, 5000) on conflict (region, item_id) do update set quantity = 5000")
            .bind(item).execute(&db).await.unwrap();
    }
    let (status, c) = api.call("POST", &format!("{CITY}/projects"), None, true, Some(proj("station", Some("smelting"), None))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    let st = c["stations"].as_array().unwrap().iter().find(|s| s["station"] == "smelting").unwrap().clone();
    assert_eq!((st["tier"].as_i64(), st["work"].as_str(), st["work_target"].as_i64()), (Some(2), Some("tier"), Some(3)));
    assert_eq!(c["stock"]["OreT1"], 2000);
    let (status, e) = api.call("POST", &format!("{CITY}/projects"), None, true, Some(proj("station", Some("smelting"), None))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("work_running")));
    // Weapon: ballista to tier 2.
    let (status, c) = api.call("POST", &format!("{CITY}/projects"), None, true, Some(proj("weapon", None, Some("ballista")))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    // 25 h later the works finish.
    let later = (time::OffsetDateTime::now_utc() + time::Duration::hours(25)).format(&time::format_description::well_known::Rfc3339).unwrap();
    let (status, _) = api.call("POST", "/internal/cities/advance", None, true, Some(json!({ "now": later }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    let st = c["stations"].as_array().unwrap().iter().find(|s| s["station"] == "smelting").unwrap().clone();
    assert_eq!((st["tier"].as_i64(), st["work"].is_null()), (Some(3), true));
    assert_eq!(c["defense"], 6);

    // Crafting tax in the character save: goes into the treasury and counts the craft at the station.
    let (_, before) = api.call("GET", CITY, None, true, None).await;
    let state = json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword", "loadout": {}, "mastery": {},
                        "city_tax": { "region": "1", "station": "smelting", "amount": 500 } });
    let (status, b) = api.call("PUT", &format!("/internal/characters/{player}/state"), None, true, Some(state)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{b}");
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    assert_eq!(c["treasury"].as_i64().unwrap(), before["treasury"].as_i64().unwrap() + 500);
    let st = c["stations"].as_array().unwrap().iter().find(|s| s["station"] == "smelting").unwrap().clone();
    assert_eq!(st["crafts"], 1);

    // City sale only after the grace period.
    let (status, e) = api.call("POST", &format!("{CITY}/sale"), None, true, Some(json!({ "character_id": gov, "price": 30_000_000 }))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("city_grace")));

    let (status, log) = api.call("GET", &format!("{CITY}/log"), None, true, None).await;
    assert_eq!(status, StatusCode::OK);
    let kinds: Vec<&str> = log.as_array().unwrap().iter().map(|l| l["kind"].as_str().unwrap()).collect();
    for k in ["owner", "raw_bought", "manage", "list_sold", "weapon", "buff", "tier", "work_done", "tax"] {
        assert!(kinds.contains(&k), "missing {k} in the log: {kinds:?}");
    }
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn cidade_semana_tenebroso_e_venda_obrigatoria(db: PgPool) {
    let api = Api::new(db.clone());
    let gov = character(&api, &db, "Paulo", "Aranha").await;
    let other = character(&api, &db, "Joao", "Besouro").await;
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    let price = c["price"].as_i64().unwrap();
    let (status, _) = api.call("POST", &format!("{CITY}/buy"), None, true, Some(json!({ "character_id": gov, "price": price, "coins": 0, "inventory": [] }))).await;
    assert_eq!(status, StatusCode::OK);

    // One week and one day with nothing: week rollover (`tenebroso`) and the first attack.
    let later = (time::OffsetDateTime::now_utc() + time::Duration::days(8)).format(&time::format_description::well_known::Rfc3339).unwrap();
    let (status, _) = api.call("POST", "/internal/cities/advance", None, true, Some(json!({ "now": later }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (tier, prize): (Option<String>, Option<i64>) = sqlx::query_as("select tier, prize from city_weeks where region = '1' and score is not null order by week limit 1")
        .fetch_one(&db).await.unwrap();
    assert_eq!((tier.as_deref(), prize), (Some("tenebroso"), Some(500_000)));
    let (_, credits) = api.call("GET", &format!("/internal/characters/{gov}/credits"), None, true, None).await;
    assert_eq!(credits["amount"], 500_000);
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    assert_eq!((c["auction"]["forced"].as_bool(), c["auction"]["start"].as_i64(), c["auction"]["min_bid"].as_i64()), (Some(true), Some(price), Some(price)));
    assert!(!c["last_attack"].is_null(), "{c}");

    // Forced auction: the owner cannot cancel; nobody buys directly; bids have a minimum and outbid bidders are refunded.
    let (status, e) = api.call("POST", &format!("{CITY}/sale"), None, true, Some(json!({ "character_id": gov, "price": null }))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("forced_sale")));
    let (status, e) = api.call("POST", &format!("{CITY}/buy"), None, true, Some(json!({ "character_id": other, "price": price, "coins": 0, "inventory": [] }))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("city_not_for_sale")));
    let third = character(&api, &db, "Maria", "Coruja").await;
    let bid = |who: &str, amount: i64| json!({ "character_id": who, "amount": amount, "coins": 0, "inventory": [] });
    let (status, e) = api.call("POST", &format!("{CITY}/bid"), None, true, Some(bid(&other, price - 1))).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::CONFLICT, Some("bid_too_low")));
    let (status, c) = api.call("POST", &format!("{CITY}/bid"), None, true, Some(bid(&other, price))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!(c["auction"]["bidder_name"], "Besouro");
    let next = c["auction"]["min_bid"].as_i64().unwrap();
    assert!(next > price);
    let (status, _) = api.call("POST", &format!("{CITY}/bid"), None, true, Some(bid(&third, next))).await;
    assert_eq!(status, StatusCode::OK);
    let (_, credits) = api.call("GET", &format!("/internal/characters/{other}/credits"), None, true, None).await;
    assert_eq!(credits["amount"], price);

    // 24 h later: the highest bidder wins; the outgoing governor receives the whole bid (forced sale).
    let end = (time::OffsetDateTime::now_utc() + time::Duration::days(8) + time::Duration::hours(25))
        .format(&time::format_description::well_known::Rfc3339).unwrap();
    let (status, _) = api.call("POST", "/internal/cities/advance", None, true, Some(json!({ "now": end }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    assert_eq!(c["owner_name"], "Coruja");
    assert!(c["auction"].is_null());
    let (_, credits) = api.call("GET", &format!("/internal/characters/{gov}/credits"), None, true, None).await;
    assert_eq!(credits["amount"], 500_000 + next);
}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn cidade_leilao_do_dono_e_sem_lance(db: PgPool) {
    let api = Api::new(db.clone());
    let gov = character(&api, &db, "Paulo", "Aranha").await;
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    let price = c["price"].as_i64().unwrap();
    let (status, _) = api.call("POST", &format!("{CITY}/buy"), None, true, Some(json!({ "character_id": gov, "price": price, "coins": 0, "inventory": [] }))).await;
    assert_eq!(status, StatusCode::OK);
    // Grace period over: the owner opens with any starting bid and may cancel with no bids.
    sqlx::query("update cities set owner_since = now() - interval '15 days'").execute(&db).await.unwrap();
    let (status, c) = api.call("POST", &format!("{CITY}/sale"), None, true, Some(json!({ "character_id": gov, "price": 1_000 }))).await;
    assert_eq!(status, StatusCode::OK, "{c}");
    assert_eq!((c["auction"]["start"].as_i64(), c["auction"]["forced"].as_bool()), (Some(1_000), Some(false)));
    let (status, c) = api.call("POST", &format!("{CITY}/sale"), None, true, Some(json!({ "character_id": gov, "price": null }))).await;
    assert_eq!(status, StatusCode::OK);
    assert!(c["auction"].is_null());
    // No bid until the end: stays with the owner.
    let (status, _) = api.call("POST", &format!("{CITY}/sale"), None, true, Some(json!({ "character_id": gov, "price": 2_000 }))).await;
    assert_eq!(status, StatusCode::OK);
    let end = (time::OffsetDateTime::now_utc() + time::Duration::hours(25)).format(&time::format_description::well_known::Rfc3339).unwrap();
    let (status, _) = api.call("POST", "/internal/cities/advance", None, true, Some(json!({ "now": end }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    let (_, c) = api.call("GET", CITY, None, true, None).await;
    assert_eq!(c["owner_name"], "Aranha");
    assert!(c["auction"].is_null());

}

#[sqlx::test(migrator = "nw_core::MIGRATOR")]
async fn teto_de_moedas_e_preco_maximo(db: PgPool) {
    let api = Api::new(db.clone());
    let id = character(&api, &db, "Paulo", "Aranha").await;
    // Save above 20 million: only up to the cap is added and the excess goes to the log.
    let state = json!({ "map": "L_QuedaEterna", "yaw": 0.0, "active_weapon": "Sword", "loadout": {}, "mastery": {}, "coins": 2_000_000_500i64 });
    let (status, b) = api.call("PUT", &format!("/internal/characters/{id}/state"), None, true, Some(state)).await;
    assert_eq!(status, StatusCode::NO_CONTENT, "{b}");
    assert_eq!(coins_of(&db, &id).await, 2_000_000_000);
    let (_, log) = api.call("GET", &format!("/internal/item-log?character_id={id}"), None, true, None).await;
    let cap = log.as_array().unwrap().iter().find(|l| l["place"] == "coins_cap").cloned().unwrap_or(Value::Null);
    assert_eq!(cap["quantity"], -500, "{log}");
    // Cut made by the game server: record only.
    let (status, _) = api.call("POST", &format!("/internal/characters/{id}/coins-cap"), None, true, Some(json!({ "amount": 700, "reason": "loot:teste" }))).await;
    assert_eq!(status, StatusCode::NO_CONTENT);
    // Market: unit price above 5 million is rejected.
    let order = json!({ "character_id": id, "kind": "buy", "item_id": "IngotT5", "quantity": 1, "unit_price": 500_000_001i64,
                        "duration_hours": 24, "coins": 0, "inventory": [] });
    let (status, e) = api.call("POST", "/internal/market/global/orders", None, true, Some(order)).await;
    assert_eq!((status, e["error"].as_str()), (StatusCode::BAD_REQUEST, Some("price_too_high")));
}

