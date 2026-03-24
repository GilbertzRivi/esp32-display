use std::path::{PathBuf, Path as FilePath};
use axum::{
    Router,
    extract::{Path, State as AxumState, WebSocketUpgrade},
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
    pub images_dir: String,
}

pub async fn run(
    host: String,
    port: u16,
    app_state: AppState,
) {
    let app = Router::new()
        .route("/layout", get(get_layout))
        .route("/theme", get(get_theme))
        .route("/images/:name", get(get_image))
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

// GET /images/<name>
// Returns binary: [u16 BE width][u16 BE height][RGB565 LE pixels...]
// Device fetches this to load image/bg_image assets.
async fn get_image(
    Path(name): Path<String>,
    AxumState(state): AxumState<Arc<AppState>>,
) -> Response<axum::body::Body> {
    tracing::info!(
        "GET /images; name={:?}, images_dir={:?}, cwd={:?}",
        name,
        state.images_dir,
        std::env::current_dir().ok()
    );

    if name.contains('/') || name.contains('\\') || name.contains("..") {
        return status_response(400);
    }

    let Some(path) = find_image_file(&state.images_dir, &name) else {
        tracing::warn!("image not found: {:?}", name);
        return status_response(404);
    };

    tracing::info!("resolved image path: {:?}", path);

    match encode_rgb565(&path) {
        Ok(data) => axum::response::Response::builder()
            .status(200)
            .header("Content-Type", "application/octet-stream")
            .header("Content-Length", data.len().to_string())
            .body(axum::body::Body::from(data))
            .unwrap(),
        Err(e) => {
            tracing::warn!("image encode error {:?}: {}", name, e);
            status_response(500)
        }
    }
}

fn status_response(code: u16) -> Response<axum::body::Body> {
    axum::response::Response::builder()
        .status(code)
        .body(axum::body::Body::empty())
        .unwrap()
}


fn find_image_file(dir: &str, name: &str) -> Option<PathBuf> {
    let mut tried = Vec::new();

    let base = FilePath::new(dir).join(name);
    tried.push(base.display().to_string());
    if base.is_file() {
        return Some(base);
    }

    for ext in ["png", "jpg", "jpeg", "bmp"] {
        let p = base.with_extension(ext);
        tried.push(p.display().to_string());
        if p.is_file() {
            return Some(p);
        }
    }

    tracing::warn!("image lookup failed; dir={dir:?}, name={name:?}, tried={tried:?}");
    None
}

// Converts any image file to the binary format expected by the device:
//   bytes 0-1:  width  (u16 big-endian)
//   bytes 2-3:  height (u16 big-endian)
//   bytes 4+:   RGB565 pixels, little-endian, row-major
fn encode_rgb565(path: &std::path::Path) -> Result<Vec<u8>, String> {
    let img = image::open(path).map_err(|e| e.to_string())?;
    let rgb = img.into_rgb8();
    let (w, h) = rgb.dimensions();

    let total = 4 + (w * h * 2) as usize;
    if total > 512 * 1024 {
        return Err(format!("image too large: {total} B (limit 512 KB)"));
    }

    let mut out = Vec::with_capacity(total);
    out.push((w >> 8) as u8);
    out.push((w & 0xff) as u8);
    out.push((h >> 8) as u8);
    out.push((h & 0xff) as u8);

    for p in rgb.pixels() {
        let r = p[0] as u16;
        let g = p[1] as u16;
        let b = p[2] as u16;
        let rgb565: u16 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        out.extend_from_slice(&rgb565.to_le_bytes());
    }

    Ok(out)
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
                socket.send(Message::Text(r#"{"__reload":1}"#.into())).await.ok();
            }
        }
    }
}
