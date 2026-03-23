use serde::Deserialize;

#[derive(Debug, Deserialize, Clone)]
pub struct Config {
    pub server: ServerConfig,
    pub artifacts: ArtifactsConfig,
    #[serde(default)]
    pub sources: Vec<SourceConfig>,
    #[serde(default)]
    pub events: Vec<EventConfig>,
}

#[derive(Debug, Deserialize, Clone)]
pub struct EventConfig {
    pub event: String,
    pub command: String,
}

#[derive(Debug, Deserialize, Clone)]
pub struct ServerConfig {
    #[serde(default = "default_host")]
    pub host: String,
    #[serde(default = "default_port")]
    pub port: u16,
}

fn default_host() -> String { "0.0.0.0".to_string() }
fn default_port() -> u16 { 1234 }

#[derive(Debug, Deserialize, Clone)]
pub struct ArtifactsConfig {
    #[serde(default = "default_layout")]
    pub layout: String,
    #[serde(default = "default_theme")]
    pub theme: String,
}

fn default_layout() -> String { "layout.json".to_string() }
fn default_theme() -> String { "theme.json".to_string() }

#[derive(Debug, Deserialize, Clone)]
pub struct SourceConfig {
    pub placeholder: String,
    pub kind: SourceKind,
    pub metric: Option<String>,
    pub command: Option<String>,
    #[serde(default = "default_interval")]
    pub interval_ms: u64,
    #[serde(default)]
    pub mode: CommandMode,
}

#[derive(Debug, Deserialize, Clone, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum SourceKind {
    Builtin,
    Command,
}

#[derive(Debug, Deserialize, Clone, Default, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum CommandMode {
    Push,
    Pull,
    #[default]
    Shot,
}

fn default_interval() -> u64 { 1000 }

impl Default for Config {
    fn default() -> Self {
        Self {
            server: ServerConfig { host: default_host(), port: default_port() },
            artifacts: ArtifactsConfig { layout: default_layout(), theme: default_theme() },
            events: vec![],
            sources: vec![
                SourceConfig {
                    placeholder: "cpu_usage".to_string(),
                    kind: SourceKind::Builtin,
                    metric: Some("cpu_percent".to_string()),
                    command: None,
                    interval_ms: 1000,
                    mode: CommandMode::Shot,
                },
                SourceConfig {
                    placeholder: "ram_percent".to_string(),
                    kind: SourceKind::Builtin,
                    metric: Some("ram_percent".to_string()),
                    command: None,
                    interval_ms: 2000,
                    mode: CommandMode::Shot,
                },
                SourceConfig {
                    placeholder: "ram_used".to_string(),
                    kind: SourceKind::Builtin,
                    metric: Some("ram_used_gb".to_string()),
                    command: None,
                    interval_ms: 2000,
                    mode: CommandMode::Shot,
                },
            ],
        }
    }
}

pub fn load(path: &str) -> Config {
    match std::fs::read_to_string(path) {
        Ok(content) => toml::from_str(&content).unwrap_or_else(|e| {
            eprintln!("config parse error: {e}, using defaults");
            Config::default()
        }),
        Err(_) => Config::default(),
    }
}
