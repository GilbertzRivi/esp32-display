use std::path::PathBuf;
use std::sync::{Arc, RwLock};
use notify::{RecommendedWatcher, RecursiveMode, Watcher, Config as NotifyConfig};
use std::sync::mpsc;

#[derive(Clone)]
pub struct Artifacts {
    pub layout_json: Arc<RwLock<String>>,
    pub theme_json: Arc<RwLock<String>>,
}

impl Artifacts {
    pub fn load(layout_path: &str, theme_path: &str) -> Self {
        let layout = std::fs::read_to_string(layout_path)
            .unwrap_or_else(|_| default_layout());
        let theme = std::fs::read_to_string(theme_path)
            .unwrap_or_else(|_| default_theme());

        Self {
            layout_json: Arc::new(RwLock::new(layout)),
            theme_json: Arc::new(RwLock::new(theme)),
        }
    }

    pub fn get_layout(&self) -> String {
        self.layout_json.read().unwrap().clone()
    }

    pub fn get_theme(&self) -> String {
        self.theme_json.read().unwrap().clone()
    }

    pub fn start_watcher(&self, layout_path: String, theme_path: String) {
        let layout_json = Arc::clone(&self.layout_json);
        let theme_json = Arc::clone(&self.theme_json);

        std::thread::spawn(move || {
            let (tx, rx) = mpsc::channel();
            let mut watcher = match RecommendedWatcher::new(tx, NotifyConfig::default()) {
                Ok(w) => w,
                Err(e) => {
                    tracing::warn!("failed to create watcher: {e}");
                    return;
                }
            };

            let layout_pb = PathBuf::from(&layout_path);
            let theme_pb = PathBuf::from(&theme_path);

            if layout_pb.exists() {
                let _ = watcher.watch(&layout_pb, RecursiveMode::NonRecursive);
            }
            if theme_pb.exists() {
                let _ = watcher.watch(&theme_pb, RecursiveMode::NonRecursive);
            }

            loop {
                match rx.recv() {
                    Ok(Ok(event)) => {
                        for path in event.paths {
                            if path == layout_pb {
                                if let Ok(content) = std::fs::read_to_string(&layout_path) {
                                    *layout_json.write().unwrap() = content;
                                    tracing::info!("hot-reloaded layout.json");
                                }
                            } else if path == theme_pb {
                                if let Ok(content) = std::fs::read_to_string(&theme_path) {
                                    *theme_json.write().unwrap() = content;
                                    tracing::info!("hot-reloaded theme.json");
                                }
                            }
                        }
                    }
                    Ok(Err(e)) => tracing::warn!("watcher error: {e}"),
                    Err(_) => break,
                }
            }
        });
    }
}

fn default_layout() -> String {
    r#"{
  "screens": [{
    "id": "main",
    "widgets": [
      { "type": "label", "layout": "absolute", "x": 0, "y": 0, "w": 480, "h": 16,
        "bind": { "text": "ESP32 Display Host" } },
      { "type": "bar", "layout": "absolute", "x": 0, "y": 20, "w": 480, "h": 20,
        "bind": { "value": "$cpu_usage", "max": 100, "label": "$cpu_usage" } },
      { "type": "bar", "layout": "absolute", "x": 0, "y": 44, "w": 480, "h": 20,
        "bind": { "value": "$ram_percent", "max": 100, "label": "$ram_used" } }
    ]
  }]
}"#.to_string()
}

fn default_theme() -> String {
    r##"{
  "palette": { "bg": "#1a1a1a", "fg": "#cacaca", "accent": "#00a4e4", "dim": "#3b3b3b", "danger": "#c25757" },
  "font": "unscii_8"
}"##.to_string()
}
