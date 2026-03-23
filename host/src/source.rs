#[allow(unused_imports)]
pub use crate::config::{SourceConfig, SourceKind, CommandMode};

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct SourceStatus {
    pub placeholder: String,
    pub kind_label: String,
    pub last_value: String,
    pub last_ok: bool,
    pub interval_ms: u64,
}
