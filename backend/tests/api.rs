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
