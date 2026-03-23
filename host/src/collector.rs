use std::time::Duration;
use tokio::time::interval;
use sysinfo::{System, Disks};
use crate::config::{SourceConfig, SourceKind, CommandMode};
use crate::state::State;

pub async fn run(
    sources: Vec<SourceConfig>,
    state: State,
    tx: tokio::sync::broadcast::Sender<()>,
) {
    let mut handles = vec![];
    for source in sources {
        let state = state.clone();
        let tx = tx.clone();
        let handle = tokio::spawn(async move {
            run_source(source, state, tx).await;
        });
        handles.push(handle);
    }

    futures::future::join_all(handles).await;
}

async fn run_source(source: SourceConfig, state: State, tx: tokio::sync::broadcast::Sender<()>) {
    match source.kind {
        SourceKind::Builtin => run_builtin(source, state, tx).await,
        SourceKind::Command => match source.mode {
            CommandMode::Push => run_command_push(source, state, tx).await,
            CommandMode::Pull => run_command_pull(source, state, tx).await,
            CommandMode::Shot => run_command_shot(source, state, tx).await,
        },
    }
}

async fn run_builtin(source: SourceConfig, state: State, tx: tokio::sync::broadcast::Sender<()>) {
    let mut ticker = interval(Duration::from_millis(source.interval_ms));
    let metric = source.metric.unwrap_or_default();

    loop {
        ticker.tick().await;
        let mut sys = System::new();

        let value = match metric.as_str() {
            "cpu_percent" => {
                sys.refresh_cpu_usage();
                tokio::time::sleep(Duration::from_millis(200)).await;
                sys.refresh_cpu_usage();
                format!("{:.0}", sys.global_cpu_usage())
            }
            m if m.starts_with("cpu_") && m.ends_with("_percent") => {
                let idx: usize = m.trim_start_matches("cpu_").trim_end_matches("_percent")
                    .parse().unwrap_or(0);
                sys.refresh_cpu_usage();
                tokio::time::sleep(Duration::from_millis(200)).await;
                sys.refresh_cpu_usage();
                let cpus = sys.cpus();
                if idx < cpus.len() {
                    format!("{:.0}", cpus[idx].cpu_usage())
                } else {
                    "0".to_string()
                }
            }
            "ram_used_gb" => {
                sys.refresh_memory();
                format!("{:.1}", sys.used_memory() as f64 / 1_073_741_824.0)
            }
            "ram_total_gb" => {
                sys.refresh_memory();
                format!("{:.1}", sys.total_memory() as f64 / 1_073_741_824.0)
            }
            "ram_percent" => {
                sys.refresh_memory();
                let pct = sys.used_memory() as f64 / sys.total_memory() as f64 * 100.0;
                format!("{:.0}", pct)
            }
            "uptime_s" => System::uptime().to_string(),
            "disk_used_gb" => {
                let disks = Disks::new_with_refreshed_list();
                let disk = disks.iter().find(|d| d.mount_point() == std::path::Path::new("/"));
                if let Some(d) = disk {
                    let used = d.total_space() - d.available_space();
                    format!("{:.1}", used as f64 / 1_073_741_824.0)
                } else {
                    "0".to_string()
                }
            }
            "disk_percent" => {
                let disks = Disks::new_with_refreshed_list();
                let disk = disks.iter().find(|d| d.mount_point() == std::path::Path::new("/"));
                if let Some(d) = disk {
                    let used = d.total_space() - d.available_space();
                    let pct = used as f64 / d.total_space() as f64 * 100.0;
                    format!("{:.0}", pct)
                } else {
                    "0".to_string()
                }
            }
            _ => "?".to_string(),
        };

        state.write().unwrap().insert(source.placeholder.clone(), value);
        let _ = tx.send(());
    }
}

async fn run_command_shot(source: SourceConfig, state: State, tx: tokio::sync::broadcast::Sender<()>) {
    let mut ticker = interval(Duration::from_millis(source.interval_ms));
    let command = source.command.unwrap_or_default();

    loop {
        ticker.tick().await;
        match tokio::process::Command::new("sh")
            .args(["-c", &command])
            .output()
            .await
        {
            Ok(output) => {
                let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
                parse_and_update(&stdout, &source.placeholder, &state);
                let _ = tx.send(());
            }
            Err(e) => tracing::warn!("shot command error: {e}"),
        }
    }
}

async fn run_command_push(source: SourceConfig, state: State, tx: tokio::sync::broadcast::Sender<()>) {
    use tokio::io::{AsyncBufReadExt, BufReader};
    use tokio::process::Command;

    let command = source.command.unwrap_or_default();

    loop {
        let child = Command::new("sh")
            .args(["-c", &command])
            .stdout(std::process::Stdio::piped())
            .spawn();

        match child {
            Ok(mut child) => {
                if let Some(stdout) = child.stdout.take() {
                    let mut lines = BufReader::new(stdout).lines();
                    loop {
                        match lines.next_line().await {
                            Ok(Some(line)) => {
                                parse_and_update(line.trim(), &source.placeholder, &state);
                                let _ = tx.send(());
                            }
                            Ok(None) => break,
                            Err(e) => {
                                tracing::warn!("push read error: {e}");
                                break;
                            }
                        }
                    }
                }
                let _ = child.wait().await;
            }
            Err(e) => tracing::warn!("push spawn error: {e}"),
        }

        tokio::time::sleep(Duration::from_secs(3)).await;
    }
}

async fn run_command_pull(source: SourceConfig, state: State, tx: tokio::sync::broadcast::Sender<()>) {
    use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
    use tokio::process::Command;

    let command = source.command.unwrap_or_default();

    loop {
        let child = Command::new("sh")
            .args(["-c", &command])
            .stdin(std::process::Stdio::piped())
            .stdout(std::process::Stdio::piped())
            .spawn();

        match child {
            Ok(mut child) => {
                if let (Some(mut stdin), Some(stdout)) = (child.stdin.take(), child.stdout.take()) {
                    let mut lines = BufReader::new(stdout).lines();
                    loop {
                        tokio::time::sleep(Duration::from_millis(source.interval_ms)).await;
                        if stdin.write_all(b"\n").await.is_err() { break; }
                        match lines.next_line().await {
                            Ok(Some(line)) => {
                                parse_and_update(line.trim(), &source.placeholder, &state);
                                let _ = tx.send(());
                            }
                            Ok(None) => break,
                            Err(_) => break,
                        }
                    }
                }
                let _ = child.wait().await;
            }
            Err(e) => tracing::warn!("pull spawn error: {e}"),
        }

        tokio::time::sleep(Duration::from_secs(3)).await;
    }
}

fn parse_and_update(line: &str, default_placeholder: &str, state: &State) {
    if let Ok(obj) = serde_json::from_str::<serde_json::Map<String, serde_json::Value>>(line) {
        for (k, v) in obj {
            let val = match &v {
                serde_json::Value::String(s) => s.clone(),
                other => other.to_string(),
            };
            state.write().unwrap().insert(k, val);
        }
    } else {
        state.write().unwrap().insert(default_placeholder.to_string(), line.to_string());
    }
}
