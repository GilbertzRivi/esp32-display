use std::time::Duration;
use tokio::time::interval;
use sysinfo::{System, Disks, Components, Networks};
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
    let metric = source.metric.clone().unwrap_or_default();

    loop {
        ticker.tick().await;
        let value = collect_builtin(&metric).await;
        parse_and_update(&value, &source.placeholder, &state);
        let _ = tx.send(());
    }
}

async fn collect_builtin(metric: &str) -> String {
    match metric {
        "cpu_percent" => {
            let mut sys = System::new();
            sys.refresh_cpu_usage();
            tokio::time::sleep(Duration::from_millis(200)).await;
            sys.refresh_cpu_usage();
            format!("{:.0}", sys.global_cpu_usage())
        }
        m if m.starts_with("cpu_") && m.ends_with("_percent") => {
            let idx: usize = m
                .trim_start_matches("cpu_")
                .trim_end_matches("_percent")
                .parse()
                .unwrap_or(0);
            let mut sys = System::new();
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
            let mut sys = System::new();
            sys.refresh_memory();
            format!("{:.1}", sys.used_memory() as f64 / 1_073_741_824.0)
        }
        "ram_total_gb" => {
            let mut sys = System::new();
            sys.refresh_memory();
            format!("{:.1}", sys.total_memory() as f64 / 1_073_741_824.0)
        }
        "ram_percent" => {
            let mut sys = System::new();
            sys.refresh_memory();
            let pct = sys.used_memory() as f64 / sys.total_memory() as f64 * 100.0;
            format!("{:.0}", pct)
        }
        "uptime_s" => System::uptime().to_string(),
        "uptime_fmt" => fmt_uptime(System::uptime()),
        "hostname" => System::host_name().unwrap_or_else(|| "unknown".to_string()),
        "kernel_short" => kernel_short(),
        "disk_used_gb" => {
            let disks = Disks::new_with_refreshed_list();
            find_system_disk(&disks)
                .map(|d| {
                    let used = d.total_space() - d.available_space();
                    format!("{:.1}", used as f64 / 1_073_741_824.0)
                })
                .unwrap_or_else(|| "0".to_string())
        }
        "disk_percent" => {
            let disks = Disks::new_with_refreshed_list();
            find_system_disk(&disks)
                .map(|d| {
                    let used = d.total_space() - d.available_space();
                    let pct = used as f64 / d.total_space() as f64 * 100.0;
                    format!("{:.0}", pct)
                })
                .unwrap_or_else(|| "0".to_string())
        }
        "cpu_temp_c" => collect_cpu_temp(),
        "gpu_stats" => collect_gpu_stats().await,
        "net_stats" => collect_net_stats().await,
        _ => "?".to_string(),
    }
}

// ── helpers ──────────────────────────────────────────────────────────────────

#[cfg(not(target_os = "windows"))]
fn kernel_short() -> String {
    System::kernel_version()
        .unwrap_or_else(|| "unknown".to_string())
        .chars()
        .take(12)
        .collect()
}

#[cfg(target_os = "windows")]
fn kernel_short() -> String {
    // kernel_version() on Windows returns NT version like "10.0.26100" — not user-friendly.
    // os_version() returns "Windows 11" or similar.
    System::os_version()
        .unwrap_or_else(|| "Windows".to_string())
        .chars()
        .take(12)
        .collect()
}

fn fmt_uptime(secs: u64) -> String {
    let d = secs / 86400;
    let h = (secs % 86400) / 3600;
    let m = (secs % 3600) / 60;
    if d > 0 {
        format!("{}d {:02}h {:02}m", d, h, m)
    } else {
        format!("{}h {:02}m", h, m)
    }
}

fn find_system_disk(disks: &Disks) -> Option<&sysinfo::Disk> {
    disks
        .iter()
        .find(|d| d.mount_point() == std::path::Path::new("/"))
        .or_else(|| {
            disks.iter().find(|d| {
                d.mount_point()
                    .to_str()
                    .map(|s| s.eq_ignore_ascii_case("c:\\"))
                    .unwrap_or(false)
            })
        })
        .or_else(|| disks.iter().max_by_key(|d| d.total_space()))
}

fn collect_cpu_temp() -> String {
    let components = Components::new_with_refreshed_list();
    let temp = components
        .iter()
        .find(|c| {
            let label = c.label().to_lowercase();
            label.contains("tctl")
                || label.contains("tdie")
                || label.contains("cpu")
                || label.contains("package")
        })
        .map(|c| c.temperature())
        .unwrap_or(0.0);
    format!("{}", temp as i32)
}

// ── GPU ───────────────────────────────────────────────────────────────────────

async fn collect_gpu_stats() -> String {
    // 1. NVIDIA via NVML — Linux + Windows
    if let Some(s) = try_nvidia_nvml() {
        return s;
    }
    // 2. AMD via sysfs — Linux
    #[cfg(target_os = "linux")]
    if let Some(s) = try_amd_sysfs() {
        return s;
    }
    // 3. AMD/Intel via PDH + DXGI — Windows (WDDM drivers, no vendor SDK needed)
    #[cfg(windows)]
    if let Some(s) = try_wddm_windows().await {
        return s;
    }
    r#"{"gpu_usage":"0","gpu_temp_c":"0","gpu_mem_used":"0.0G"}"#.to_string()
}

fn try_nvidia_nvml() -> Option<String> {
    use nvml_wrapper::enum_wrappers::device::TemperatureSensor;
    use nvml_wrapper::Nvml;

    let nvml = Nvml::init().ok()?;
    let device = nvml.device_by_index(0).ok()?;
    let util = device.utilization_rates().ok()?;
    let temp = device.temperature(TemperatureSensor::Gpu).ok()?;
    let mem = device.memory_info().ok()?;
    let mem_used_gb = mem.used as f64 / 1_073_741_824.0;

    Some(format!(
        r#"{{"gpu_usage":"{}","gpu_temp_c":"{}","gpu_mem_used":"{:.1}G"}}"#,
        util.gpu, temp, mem_used_gb
    ))
}

#[cfg(target_os = "linux")]
fn try_amd_sysfs() -> Option<String> {
    use std::fs;

    let card = (0..8u8).find_map(|i| {
        let p = format!("/sys/class/drm/card{}/device/uevent", i);
        let content = fs::read_to_string(&p).ok()?;
        content.contains("DRIVER=amdgpu").then(|| format!("/sys/class/drm/card{}", i))
    })?;

    let gpu_usage = fs::read_to_string(format!("{}/device/gpu_busy_percent", card))
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|_| "0".to_string());

    let gpu_temp_c = fs::read_dir(format!("{}/device/hwmon", card))
        .ok()
        .and_then(|mut d| d.next()?.ok())
        .and_then(|entry| fs::read_to_string(entry.path().join("temp1_input")).ok())
        .and_then(|s| s.trim().parse::<u64>().ok())
        .map(|v| (v / 1000).to_string())
        .unwrap_or_else(|| "0".to_string());

    let gpu_mem_used = fs::read_to_string(format!("{}/device/mem_info_vram_used", card))
        .ok()
        .and_then(|s| s.trim().parse::<u64>().ok())
        .map(|n| format!("{:.1}G", n as f64 / 1_073_741_824.0))
        .unwrap_or_else(|| "0.0G".to_string());

    Some(format!(
        r#"{{"gpu_usage":"{}","gpu_temp_c":"{}","gpu_mem_used":"{}"}}"#,
        gpu_usage, gpu_temp_c, gpu_mem_used
    ))
}

// Windows: PDH counters (same source as Task Manager) + DXGI for VRAM.
// Works for AMD, NVIDIA and Intel — anything with WDDM drivers.
// Temperature not available via standard Windows API without vendor SDK.
#[cfg(windows)]
async fn try_wddm_windows() -> Option<String> {
    let util = tokio::task::spawn_blocking(pdh_gpu_utilization)
        .await
        .unwrap_or(0);
    let mem_gb = tokio::task::spawn_blocking(dxgi_gpu_memory_gb)
        .await
        .unwrap_or(0.0);
    Some(format!(
        r#"{{"gpu_usage":"{}","gpu_temp_c":"N/A","gpu_mem_used":"{:.1}G"}}"#,
        util, mem_gb
    ))
}

// Sum of \GPU Engine(*engtype_3D)\Utilization Percentage across all processes —
// equivalent to what Task Manager aggregates for total GPU busy %.
#[cfg(windows)]
fn pdh_gpu_utilization() -> u32 {
    use windows::Win32::System::Performance::*;
    use windows::core::PCWSTR;

    unsafe {
        let mut query = PDH_HQUERY::default();
        if PdhOpenQueryW(PCWSTR::null(), 0, &mut query).is_err() {
            return 0;
        }

        let counter_path: Vec<u16> =
            "\\GPU Engine(*engtype_3D)\\Utilization Percentage\0"
                .encode_utf16()
                .collect();
        let mut counter = PDH_HCOUNTER::default();
        if PdhAddCounterW(query, PCWSTR(counter_path.as_ptr()), 0, &mut counter).is_err() {
            PdhCloseQuery(query);
            return 0;
        }

        // PDH rate counters need two collections to compute a delta.
        let _ = PdhCollectQueryData(query);
        std::thread::sleep(std::time::Duration::from_millis(200));
        let _ = PdhCollectQueryData(query);

        // First call: get required buffer size and instance count.
        let mut buf_bytes = 0u32;
        let mut item_count = 0u32;
        let _ = PdhGetFormattedCounterArrayW(
            counter,
            PDH_FMT_DOUBLE,
            &mut buf_bytes,
            &mut item_count,
            None,
        );

        let total = if item_count > 0 && buf_bytes > 0 {
            let mut buf = vec![0u8; buf_bytes as usize];
            let item_ptr = buf.as_mut_ptr() as *mut PDH_FMT_COUNTERVALUE_ITEM_W;
            if PdhGetFormattedCounterArrayW(
                counter,
                PDH_FMT_DOUBLE,
                &mut buf_bytes,
                &mut item_count,
                Some(item_ptr),
            )
            .is_ok()
            {
                let items = std::slice::from_raw_parts(item_ptr, item_count as usize);
                items
                    .iter()
                    .map(|it| it.FmtValue.Anonymous.doubleValue)
                    .sum::<f64>()
            } else {
                0.0
            }
        } else {
            0.0
        };

        PdhCloseQuery(query);
        total.min(100.0) as u32
    }
}

// IDXGIAdapter3::QueryVideoMemoryInfo — dedicated (local) VRAM usage in GB.
#[cfg(windows)]
fn dxgi_gpu_memory_gb() -> f64 {
    use windows::Win32::Graphics::Dxgi::*;
    use windows::Win32::Graphics::Dxgi::Common::DXGI_MEMORY_SEGMENT_GROUP_LOCAL;

    unsafe {
        let factory: IDXGIFactory1 = match CreateDXGIFactory1() {
            Ok(f) => f,
            Err(_) => return 0.0,
        };
        let adapter = match factory.EnumAdapters(0) {
            Ok(a) => a,
            Err(_) => return 0.0,
        };
        let adapter3: IDXGIAdapter3 = match adapter.cast() {
            Ok(a) => a,
            Err(_) => return 0.0,
        };
        match adapter3.QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL) {
            Ok(info) => info.CurrentUsage as f64 / 1_073_741_824.0,
            Err(_) => 0.0,
        }
    }
}

// ── Network ───────────────────────────────────────────────────────────────────

async fn collect_net_stats() -> String {
    use std::net::UdpSocket;

    // Detect outgoing IP via UDP connect trick (no packets sent)
    let default_ip = UdpSocket::bind("0.0.0.0:0")
        .and_then(|s| {
            s.connect("8.8.8.8:80")?;
            s.local_addr()
        })
        .map(|a| a.ip().to_string())
        .unwrap_or_else(|_| "0.0.0.0".to_string());

    // Two measurements 1s apart for byte rates
    let mut nets = Networks::new_with_refreshed_list();
    nets.refresh();
    tokio::time::sleep(Duration::from_secs(1)).await;
    nets.refresh();

    // Find interface whose IP matches our detected outgoing IP
    let iface_name: String = nets
        .iter()
        .find(|(_, net)| {
            net.ip_networks()
                .iter()
                .any(|ip| ip.addr.to_string() == default_ip)
        })
        .or_else(|| {
            nets.iter()
                .find(|(name, _)| *name != "lo" && !name.starts_with("loop"))
        })
        .map(|(name, _)| name.clone())
        .unwrap_or_default();

    let (rx_bps, tx_bps, ip4) = nets
        .iter()
        .find(|(name, _)| **name == iface_name)
        .map(|(_, net)| {
            let ip = net
                .ip_networks()
                .iter()
                .find(|ip| ip.addr.is_ipv4())
                .map(|ip| ip.addr.to_string())
                .unwrap_or_else(|| default_ip.clone());
            (net.received(), net.transmitted(), ip)
        })
        .unwrap_or((0, 0, default_ip));

    let speed_mbps = read_link_speed_mbps(&iface_name);
    let link_bps = speed_mbps * 125_000u64;
    let rx_pct = if link_bps > 0 { (rx_bps * 100 / link_bps).min(100) } else { 0 };
    let tx_pct = if link_bps > 0 { (tx_bps * 100 / link_bps).min(100) } else { 0 };

    let link_label = if speed_mbps >= 1000 {
        if speed_mbps % 1000 == 0 {
            format!("{}G", speed_mbps / 1000)
        } else {
            format!("{:.1}G", speed_mbps as f64 / 1000.0)
        }
    } else {
        format!("{}M", speed_mbps)
    };

    let gateway = read_default_gateway(&iface_name);
    let state = read_operstate(&iface_name);

    format!(
        r#"{{"net_ip":"{}","net_rx":"{}","net_tx":"{}","net_iface":"{}","net_gateway":"{}","net_state":"{}","net_link":"{}","net_rx_bps":"{}","net_tx_bps":"{}","net_link_bps":"{}","net_rx_pct":"{}","net_tx_pct":"{}","net_graph_scale":"{}"}}"#,
        ip4,
        fmt_rate(rx_bps),
        fmt_rate(tx_bps),
        iface_name,
        gateway,
        state,
        link_label,
        rx_bps,
        tx_bps,
        link_bps,
        rx_pct,
        tx_pct,
        link_label,
    )
}

fn fmt_rate(bps: u64) -> String {
    if bps >= 1_073_741_824 {
        format!("{:.2} GB/s", bps as f64 / 1_073_741_824.0)
    } else if bps >= 1_048_576 {
        format!("{:.1} MB/s", bps as f64 / 1_048_576.0)
    } else if bps >= 1024 {
        format!("{:.0} KB/s", bps as f64 / 1024.0)
    } else {
        format!("{} B/s", bps)
    }
}

#[cfg(target_os = "linux")]
fn read_link_speed_mbps(iface: &str) -> u64 {
    std::fs::read_to_string(format!("/sys/class/net/{}/speed", iface))
        .ok()
        .and_then(|s| s.trim().parse::<i64>().ok())
        .filter(|&v| v > 0)
        .map(|v| v as u64)
        .unwrap_or(1000)
}

#[cfg(not(target_os = "linux"))]
fn read_link_speed_mbps(_iface: &str) -> u64 {
    1000
}

#[cfg(target_os = "linux")]
fn read_default_gateway(iface: &str) -> String {
    let content = std::fs::read_to_string("/proc/net/route").unwrap_or_default();
    for line in content.lines().skip(1) {
        let mut fields = line.split_whitespace();
        let iface_field = fields.next().unwrap_or("");
        let dest = fields.next().unwrap_or("");
        let gw_hex = fields.next().unwrap_or("");
        if iface_field == iface && dest == "00000000" {
            if let Ok(gw) = u32::from_str_radix(gw_hex, 16) {
                let b = gw.to_le_bytes();
                return format!("{}.{}.{}.{}", b[0], b[1], b[2], b[3]);
            }
        }
    }
    "-".to_string()
}

#[cfg(not(target_os = "linux"))]
fn read_default_gateway(_iface: &str) -> String {
    "-".to_string()
}

#[cfg(target_os = "linux")]
fn read_operstate(iface: &str) -> String {
    std::fs::read_to_string(format!("/sys/class/net/{}/operstate", iface))
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|_| "unknown".to_string())
}

#[cfg(not(target_os = "linux"))]
fn read_operstate(_iface: &str) -> String {
    "unknown".to_string()
}

// ── Command runners ───────────────────────────────────────────────────────────

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
                        if stdin.write_all(b"\n").await.is_err() {
                            break;
                        }
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
