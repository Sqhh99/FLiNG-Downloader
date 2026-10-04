//! Repository tasks: `cargo xtask <task>`.
//!
//! - `dist [--version X.Y.Z] [--out DIR]`: build the release app and lay out
//!   the portable/installer folder (`dist/FLiNG Downloader` by default):
//!
//!   ```text
//!   FLiNG Downloader.exe
//!   models/game-cover-v2.onnx
//!   resources/fling_translations.db
//!   vcruntime140.dll, vcruntime140_1.dll, msvcp140.dll, msvcp140_1.dll
//!   LICENSE, THIRD_PARTY_NOTICES.md
//!   ```
//!
//!   ONNX Runtime is linked into the exe; the MSVC runtime DLLs are copied
//!   app-local so the package runs without the VC++ redistributable.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

const APP_DIR_NAME: &str = "FLiNG Downloader";
const APP_EXE: &str = "FLiNG Downloader.exe";
const CRT_DLLS: [&str; 4] = [
    "vcruntime140.dll",
    "vcruntime140_1.dll",
    "msvcp140.dll",
    "msvcp140_1.dll",
];

type Result<T> = std::result::Result<T, String>;

fn main() -> ExitCode {
    let args: Vec<String> = env::args().skip(1).collect();
    let result = match args.first().map(String::as_str) {
        Some("dist") => dist(&args[1..]),
        _ => Err("usage: cargo xtask dist [--version X.Y.Z] [--out DIR]".to_owned()),
    };
    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("error: {err}");
            ExitCode::FAILURE
        }
    }
}

fn repo_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("xtask lives in the repo")
        .to_path_buf()
}

fn flag_value(args: &[String], name: &str) -> Result<Option<String>> {
    match args.iter().position(|a| a == name) {
        Some(i) => args
            .get(i + 1)
            .cloned()
            .map(Some)
            .ok_or(format!("{name} needs a value")),
        None => Ok(None),
    }
}

fn copy(from: &Path, to: &Path) -> Result<()> {
    if let Some(dir) = to.parent() {
        fs::create_dir_all(dir).map_err(|e| format!("create {}: {e}", dir.display()))?;
    }
    fs::copy(from, to)
        .map(|_| ())
        .map_err(|e| format!("copy {} -> {}: {e}", from.display(), to.display()))
}

fn dist(args: &[String]) -> Result<()> {
    let root = repo_root();
    let version = flag_value(args, "--version")?;
    let out = flag_value(args, "--out")?
        .map(PathBuf::from)
        .unwrap_or_else(|| root.join("dist").join(APP_DIR_NAME));

    let mut build = Command::new(env::var("CARGO").unwrap_or_else(|_| "cargo".into()));
    build
        .current_dir(&root)
        .args(["build", "--release", "-p", "fling-ui"]);
    if let Some(version) = &version {
        build.env("FLING_APP_VERSION", version);
    }
    let status = build.status().map_err(|e| format!("run cargo: {e}"))?;
    if !status.success() {
        return Err("cargo build failed".into());
    }

    if out.exists() {
        fs::remove_dir_all(&out).map_err(|e| format!("clear {}: {e}", out.display()))?;
    }
    copy(
        &root.join("target/release/fling-downloader.exe"),
        &out.join(APP_EXE),
    )?;
    copy(
        &root.join("resources/models/game-cover-v2.onnx"),
        &out.join("models/game-cover-v2.onnx"),
    )?;
    copy(
        &root.join("resources/fling_translations.db"),
        &out.join("resources/fling_translations.db"),
    )?;
    copy(&root.join("LICENSE"), &out.join("LICENSE"))?;
    copy(
        &root.join("THIRD_PARTY_NOTICES.md"),
        &out.join("THIRD_PARTY_NOTICES.md"),
    )?;

    let crt = find_crt_dir()?;
    for dll in CRT_DLLS {
        copy(&crt.join(dll), &out.join(dll))?;
    }

    let total: u64 = walk(&out)
        .iter()
        .filter_map(|p| fs::metadata(p).ok())
        .map(|m| m.len())
        .sum();
    println!(
        "dist: {} ({:.1} MB)",
        out.display(),
        total as f64 / (1024.0 * 1024.0)
    );
    println!("  MSVC runtime from {}", crt.display());
    Ok(())
}

fn walk(dir: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                files.extend(walk(&path));
            } else {
                files.push(path);
            }
        }
    }
    files
}

/// The x64 `Microsoft.VC14x.CRT` redist folder: `VCToolsRedistDir` (set in a
/// VS developer shell and by `ilammy/msvc-dev-cmd`), else the newest Visual
/// Studio found by `vswhere`.
fn find_crt_dir() -> Result<PathBuf> {
    let mut roots = Vec::new();
    if let Ok(dir) = env::var("VCToolsRedistDir") {
        roots.push(PathBuf::from(dir));
    }
    if let Some(install) = vswhere_installation() {
        let msvc = install.join(r"VC\Redist\MSVC");
        let mut versions: Vec<PathBuf> = fs::read_dir(&msvc)
            .map(|e| {
                e.flatten()
                    .map(|e| e.path())
                    .filter(|p| p.is_dir())
                    .collect()
            })
            .unwrap_or_default();
        versions.sort();
        roots.extend(versions.into_iter().rev());
    }
    for root in roots {
        let x64 = root.join("x64");
        let Ok(entries) = fs::read_dir(&x64) else {
            continue;
        };
        let mut crts: Vec<PathBuf> = entries
            .flatten()
            .map(|e| e.path())
            .filter(|p| {
                let name = p
                    .file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_default();
                name.starts_with("Microsoft.VC") && name.ends_with(".CRT")
            })
            .collect();
        crts.sort();
        if let Some(crt) = crts
            .pop()
            .filter(|c| CRT_DLLS.iter().all(|d| c.join(d).is_file()))
        {
            return Ok(crt);
        }
    }
    Err("MSVC runtime redist not found; run from a VS developer shell or install the C++ build tools".into())
}

fn vswhere_installation() -> Option<PathBuf> {
    let program_files = env::var("ProgramFiles(x86)").ok()?;
    let vswhere = Path::new(&program_files).join(r"Microsoft Visual Studio\Installer\vswhere.exe");
    let output = Command::new(vswhere)
        .args([
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        ])
        .args(["-property", "installationPath"])
        .output()
        .ok()?;
    let path = String::from_utf8(output.stdout).ok()?.trim().to_owned();
    (!path.is_empty()).then(|| PathBuf::from(path))
}
