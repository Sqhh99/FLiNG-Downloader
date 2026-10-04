//! Derives the app version from git (like the Qt build's CMake step) and, on
//! Windows, embeds the icon and version resource.
//!
//! `FLING_APP_VERSION` in the environment overrides the git-derived value.

use std::process::Command;

fn git_version() -> Option<String> {
    let output = Command::new("git")
        .args(["describe", "--tags", "--match", "v[0-9]*", "--always"])
        .output()
        .ok()?;
    let describe = String::from_utf8(output.stdout).ok()?;
    let describe = describe.trim().strip_prefix('v')?;
    let is_core =
        |s: &str| s.split('.').count() == 3 && s.split('.').all(|p| p.parse::<u32>().is_ok());
    if is_core(describe) {
        return Some(describe.to_owned());
    }
    // vX.Y.Z-N-gHASH -> X.Y.Z-dev.N+gHASH
    let mut parts = describe.rsplitn(3, '-');
    let (hash, commits, core) = (parts.next()?, parts.next()?, parts.next()?);
    (is_core(core) && commits.parse::<u32>().is_ok() && hash.starts_with('g'))
        .then(|| format!("{core}-dev.{commits}+{hash}"))
}

fn main() {
    println!("cargo:rerun-if-env-changed=FLING_APP_VERSION");
    // HEAD only names the branch; the reflog changes on every commit/checkout.
    println!("cargo:rerun-if-changed=../../.git/HEAD");
    println!("cargo:rerun-if-changed=../../.git/logs/HEAD");
    println!("cargo:rerun-if-changed=../../.git/refs/tags");
    let version = std::env::var("FLING_APP_VERSION")
        .ok()
        .filter(|v| !v.is_empty())
        .or_else(git_version)
        .unwrap_or_else(|| "0.0.0-dev".to_owned());
    println!("cargo:rustc-env=FLING_APP_VERSION={version}");

    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        let numeric: Vec<u64> = version
            .split(['-', '+'])
            .next()
            .unwrap_or_default()
            .split('.')
            .map(|p| p.parse().unwrap_or(0))
            .collect();
        let packed = numeric.iter().take(3).fold(0u64, |acc, n| (acc << 16) | n) << 16;
        let mut res = winresource::WindowsResource::new();
        res.set_icon("../../resources/icons/app_icon.ico")
            .set("ProductName", "FLiNG Downloader")
            .set("FileDescription", "FLiNG Downloader")
            .set("ProductVersion", &version)
            .set("FileVersion", &version)
            .set_version_info(winresource::VersionInfo::PRODUCTVERSION, packed)
            .set_version_info(winresource::VersionInfo::FILEVERSION, packed);
        if let Err(err) = res.compile() {
            println!("cargo:warning=failed to embed Windows resources: {err}");
        }
    }
}
