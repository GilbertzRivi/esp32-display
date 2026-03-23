use axum::{
    Router,
    extract::{State as AxumState, WebSocketUpgrade},
    extract::ws::{Message, WebSocket},
    response::Response,
    routing::get,
};
use std::sync::Arc;
use tokio::sync::broadcast;

#[derive(Clone)]
pub struct AppState {
    pub artifacts: crate::artifacts::Artifacts,
    pub state: crate::state::State,
    pub broadcast_tx: broadcast::Sender<()>,
    pub events: Vec<crate::config::EventConfig>,
    pub reload_tx: broadcast::Sender<()>,
}

pub async fn run(
    host: String,
    port: u16,
    app_state: AppState,
) {
    let app = Router::new()
        .route("/layout", get(get_layout))
        .route("/theme", get(get_theme))
        .route("/ws", get(ws_handler))
        .with_state(Arc::new(app_state));

    let addr = format!("{host}:{port}");
    tracing::info!("server listening on {addr}");
    let listener = tokio::net::TcpListener::bind(&addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

async fn get_layout(AxumState(state): AxumState<Arc<AppState>>) -> Response<String> {
    axum::response::Response::builder()
        .header("Content-Type", "application/json")
        .body(state.artifacts.get_layout())
        .unwrap()
}

async fn get_theme(AxumState(state): AxumState<Arc<AppState>>) -> Response<String> {
    axum::response::Response::builder()
        .header("Content-Type", "application/json")
        .body(state.artifacts.get_theme())
        .unwrap()
}

async fn ws_handler(
    ws: WebSocketUpgrade,
    AxumState(state): AxumState<Arc<AppState>>,
) -> Response {
    ws.on_upgrade(|socket| handle_ws(socket, state))
}

async fn handle_ws(mut socket: WebSocket, state: Arc<AppState>) {
    let mut rx = state.broadcast_tx.subscribe();
    let mut reload_rx = state.reload_tx.subscribe();

    // Send current state immediately
    let json = crate::state::to_json(&state.state);
    if socket.send(Message::Text(json.into())).await.is_err() {
        return;
    }

    loop {
        tokio::select! {
            result = rx.recv() => {
                match result {
                    Ok(_) => {
                        let json = crate::state::to_json(&state.state);
                        if socket.send(Message::Text(json.into())).await.is_err() {
                            break;
                        }
                    }
                    Err(broadcast::error::RecvError::Lagged(_)) => continue,
                    Err(_) => break,
                }
            }
            msg = socket.recv() => {
                match msg {
                    Some(Ok(Message::Text(text))) => {
                        tracing::info!("device event: {text}");
                        if let Ok(v) = serde_json::from_str::<serde_json::Value>(&text) {
                            if let Some(ev) = v["event"].as_str() {
                                if let Some(cfg) = state.events.iter().find(|e| e.event == ev) {
                                    tokio::process::Command::new("sh")
                                        .args(["-c", &cfg.command])
                                        .spawn()
                                        .ok();
                                }
                            }
                        }
                    }
                    Some(Ok(Message::Close(_))) | None => break,
                    _ => {}
                }
            }
            _ = reload_rx.recv() => {
                socket.send(Message::Text(r#"{"__reload":"1"}"#.into())).await.ok();
            }
        }
    }
}
