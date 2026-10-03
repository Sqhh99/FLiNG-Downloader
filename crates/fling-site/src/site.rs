//! Search, detail and recently-updated workflows. Port of `SearchManager` and
//! `ModifierManager::getModifierDetail`.

use std::path::Path;
use std::sync::Arc;

use fling_core::ModifierInfo;
use fling_core::text::format_modifier_name;
use fling_mapping::GameMappings;
use fling_net::{GetError, HttpClient, get_text};
use futures::StreamExt;

use crate::parser::{
    contains_ci, lazy_re, parse_featured, parse_modifier_detail, parse_modifier_list,
    parse_recently_updated,
};
use crate::recent_cache::{load_recent_cache, same_recent_list, save_recent_cache};

pub const DEFAULT_BASE_URL: &str = "https://flingtrainer.com/";

/// Homepage fetch attempts before giving up on the recent list.
const RECENT_ATTEMPTS: usize = 3;
/// Detail pages fetched at once when enriching search results.
const ENRICH_CONCURRENCY: usize = 8;

lazy_re!(ENRICH_OPTIONS, r"(?i)(\d+)\s*Options");
lazy_re!(ENRICH_VERSION, r"(?i)Game Version:\s*([^<\n·]+)");

/// flingtrainer.com over an [`HttpClient`].
#[derive(Clone)]
pub struct SiteClient {
    http: Arc<dyn HttpClient>,
    base_url: String,
}

impl SiteClient {
    pub fn new(http: Arc<dyn HttpClient>) -> Self {
        Self::with_base_url(http, DEFAULT_BASE_URL)
    }

    /// `base_url` must end with `/`.
    pub fn with_base_url(http: Arc<dyn HttpClient>, base_url: &str) -> Self {
        Self {
            http,
            base_url: base_url.to_owned(),
        }
    }

    pub fn home_url(&self) -> &str {
        &self.base_url
    }

    /// `{base}?s=term`, with only spaces encoded (as `+`), as in the Qt build.
    pub fn search_url(&self, term: &str) -> String {
        format!("{}?s={}", self.base_url, term.replace(' ', "+"))
    }

    /// Searches the site. CN/JA titles with an exact database match are
    /// searched by their English title; an empty input loads the homepage's
    /// featured trainers.
    ///
    /// Results are relevance-sorted and their options count / game version are
    /// filled in from detail pages.
    pub async fn search(
        &self,
        input: &str,
        mappings: &GameMappings,
    ) -> Result<Vec<ModifierInfo>, GetError> {
        if input.is_empty() {
            return self.featured().await;
        }
        let term = mappings
            .translate_for_search(input)
            .unwrap_or_else(|| input.to_owned());
        let html = get_text(&*self.http, &self.search_url(&term)).await?;

        let mut list = parse_modifier_list(&html, &term);
        for modifier in &mut list {
            modifier.name = format_modifier_name(&modifier.name);
        }
        sort_by_relevance(&mut list, &term);
        if list
            .iter()
            .any(|m| m.options_count == 0 || m.game_version.is_empty())
        {
            self.enrich(&mut list).await;
        }
        Ok(list)
    }

    /// The homepage "Latest Trainers" section (empty-search behavior).
    pub async fn featured(&self) -> Result<Vec<ModifierInfo>, GetError> {
        let html = get_text(&*self.http, &self.base_url).await?;
        let today = chrono::Local::now().format("%Y-%m-%d").to_string();
        Ok(parse_featured(&html, &today))
    }

    /// A trainer's detail page: versions, options, metadata and screenshot.
    pub async fn detail(&self, url: &str) -> Result<ModifierInfo, GetError> {
        let html = get_text(&*self.http, url).await?;
        let mut detail = parse_modifier_detail(&html);
        detail.url = url.to_owned();
        Ok(detail)
    }

    /// The homepage "recently updated" list with a cache in front of it.
    ///
    /// `deliver` is called with the cached list at once (if there is one), then
    /// again with the network result only when it differs. With no cache and no
    /// network result after three attempts it is called with an empty list.
    pub async fn fetch_recent(
        &self,
        cache_path: &Path,
        mut deliver: impl FnMut(Vec<ModifierInfo>),
    ) {
        let cached = load_recent_cache(cache_path);
        if !cached.is_empty() {
            deliver(cached.clone());
        }

        let mut fresh = None;
        for attempt in 1..=RECENT_ATTEMPTS {
            match get_text(&*self.http, &self.base_url).await {
                Ok(html) => {
                    let list = parse_recently_updated(&html);
                    if !list.is_empty() {
                        save_recent_cache(cache_path, &list);
                        fresh = Some(list);
                        break;
                    }
                    tracing::warn!(attempt, "recently updated list parsed empty");
                }
                Err(err) => tracing::warn!(attempt, %err, "failed to fetch recently updated list"),
            }
        }

        match fresh {
            Some(list) if cached.is_empty() || !same_recent_list(&list, &cached) => deliver(list),
            Some(_) => {}
            None if cached.is_empty() => deliver(Vec::new()),
            None => tracing::warn!("keeping cached recently updated list"),
        }
    }

    /// Fills missing options counts and game versions from detail pages.
    async fn enrich(&self, list: &mut [ModifierInfo]) {
        let jobs: Vec<(usize, String)> = list
            .iter()
            .enumerate()
            .filter(|(_, m)| {
                !(m.options_count > 0 && !m.game_version.is_empty()) && !m.url.is_empty()
            })
            .map(|(i, m)| (i, m.url.clone()))
            .collect();

        let mut results = futures::stream::iter(jobs)
            .map(|(index, url)| {
                let http = self.http.clone();
                async move { (index, get_text(&*http, &url).await) }
            })
            .buffer_unordered(ENRICH_CONCURRENCY);

        while let Some((index, page)) = results.next().await {
            let Ok(html) = page else {
                continue;
            };
            let options = ENRICH_OPTIONS
                .captures(&html)
                .and_then(|c| c[1].parse::<u32>().ok())
                .unwrap_or(0);
            let version = ENRICH_VERSION
                .captures(&html)
                .map(|c| c[1].trim().to_owned())
                .unwrap_or_default();
            let modifier = &mut list[index];
            if options > 0 {
                modifier.options_count = options;
            }
            if !version.is_empty() {
                modifier.game_version = version;
            }
        }
    }
}

/// Stable sort, most relevant first: +50 when the name contains `term`, +50
/// more for an exact (case-insensitive) match, +30 when the game version
/// contains it.
pub fn sort_by_relevance(list: &mut [ModifierInfo], term: &str) {
    let score = |m: &ModifierInfo| {
        let mut score = 0;
        if contains_ci(&m.name, term) {
            score += 50;
            if m.name.to_lowercase() == term.to_lowercase() {
                score += 50;
            }
        }
        if contains_ci(&m.game_version, term) {
            score += 30;
        }
        score
    };
    list.sort_by_key(|m| std::cmp::Reverse(score(m)));
}

#[cfg(test)]
mod tests {
    use fling_mapping::GameRecord;
    use fling_net::fake::FakeHttpClient;

    use super::*;

    const BASE: &str = "https://fling.test/";

    fn mappings() -> GameMappings {
        GameMappings::from_records(&[GameRecord {
            english: "Ace Combat 7: Skies Unknown".into(),
            normalized_english: "ace combat 7 skies unknown".into(),
            chinese_simplified: "皇牌空战7：未知空域".into(),
            japanese: "エースコンバット7 スカイズ・アンノウン".into(),
        }])
    }

    fn search_page(entries: &[(&str, &str)]) -> String {
        let articles: String = entries
            .iter()
            .map(|(title, href)| {
                format!(
                    r#"<article class="post"><h2 class="post-title"><a href="{href}">{title}</a></h2></article>"#
                )
            })
            .collect();
        format!("<html>SEARCH RESULTS {articles}</html>")
    }

    fn client(fake: &FakeHttpClient) -> SiteClient {
        SiteClient::with_base_url(Arc::new(fake.clone()), BASE)
    }

    #[tokio::test]
    async fn japanese_input_searches_english_title() {
        let fake = FakeHttpClient::new();
        fake.on_get(|_| Some(Ok(b"<html></html>".to_vec())));
        client(&fake)
            .search("エースコンバット7 スカイズ・アンノウン", &mappings())
            .await
            .unwrap();
        assert_eq!(
            fake.requested_gets(),
            [format!("{BASE}?s=Ace+Combat+7:+Skies+Unknown")]
        );
    }

    #[tokio::test]
    async fn broad_latin_query_stays_a_site_search() {
        let fake = FakeHttpClient::new();
        fake.on_get(|_| Some(Ok(b"<html></html>".to_vec())));
        client(&fake)
            .search("ace combat", &mappings())
            .await
            .unwrap();
        assert_eq!(fake.requested_gets(), [format!("{BASE}?s=ace+combat")]);
    }

    #[tokio::test]
    async fn search_formats_sorts_and_enriches() {
        let fake = FakeHttpClient::new();
        fake.page(
            &format!("{BASE}?s=elden+ring"),
            search_page(&[
                (
                    "elden ring nightreign trainer",
                    "https://fling.test/a-trainer/",
                ),
                ("elden ring", "https://fling.test/b-trainer/"),
            ]),
        );
        fake.page(
            "https://fling.test/a-trainer/",
            "<p>18 Options</p><p>Game Version: v1.0+ · Steam</p>",
        );
        fake.page_error("https://fling.test/b-trainer/", GetError::Status(500));

        let list = client(&fake)
            .search("elden ring", &GameMappings::default())
            .await
            .unwrap();
        let names: Vec<_> = list.iter().map(|m| m.name.as_str()).collect();
        // Exact match first, then the containing title.
        assert_eq!(names, ["Elden Ring", "Elden Ring Nightreign Trainer"]);
        assert_eq!(list[1].options_count, 18);
        assert_eq!(list[1].game_version, "v1.0+");
        assert_eq!(
            list[0].options_count, 0,
            "failed detail fetch leaves the row as is"
        );
    }

    #[tokio::test]
    async fn search_failure_is_an_error() {
        let fake = FakeHttpClient::new();
        let result = client(&fake).search("x", &GameMappings::default()).await;
        assert_eq!(result, Err(GetError::Status(404)));
    }

    #[tokio::test]
    async fn detail_keeps_url() {
        let fake = FakeHttpClient::new();
        fake.page("https://fling.test/d/", "<p>Options: 7</p>");
        let detail = client(&fake).detail("https://fling.test/d/").await.unwrap();
        assert_eq!(detail.url, "https://fling.test/d/");
        assert_eq!(detail.options_count, 7);
    }

    fn recent_page(name: &str) -> String {
        format!(
            r#"<article class="post-standard"><h2 class="post-title"><a href="https://fling.test/{name}/">{name} trainer</a></h2></article>"#
        )
    }

    #[tokio::test]
    async fn recent_delivers_cache_then_changed_network_list() {
        let dir = tempfile::tempdir().unwrap();
        let cache = dir.path().join("recent.json");
        let fake = FakeHttpClient::new();
        fake.page(BASE, recent_page("alpha"));

        let mut deliveries = Vec::new();
        client(&fake)
            .fetch_recent(&cache, |l| deliveries.push(l))
            .await;
        assert_eq!(deliveries.len(), 1, "no cache: network list only");
        assert_eq!(deliveries[0][0].name, "Alpha Trainer");

        // Same page again: cached list shown, unchanged network list not re-delivered.
        deliveries.clear();
        client(&fake)
            .fetch_recent(&cache, |l| deliveries.push(l))
            .await;
        assert_eq!(deliveries.len(), 1);

        // Changed page: cache first, then the new list.
        fake.page(BASE, recent_page("beta"));
        deliveries.clear();
        client(&fake)
            .fetch_recent(&cache, |l| deliveries.push(l))
            .await;
        assert_eq!(deliveries.len(), 2);
        assert_eq!(deliveries[1][0].name, "Beta Trainer");
    }

    #[tokio::test]
    async fn recent_retries_then_gives_up() {
        let dir = tempfile::tempdir().unwrap();
        let cache = dir.path().join("recent.json");
        let fake = FakeHttpClient::new();
        let mut deliveries = Vec::new();
        client(&fake)
            .fetch_recent(&cache, |l| deliveries.push(l))
            .await;
        assert_eq!(fake.requested_gets().len(), 3);
        assert_eq!(deliveries, [Vec::<ModifierInfo>::new()]);
    }

    #[test]
    fn relevance_is_stable_for_ties() {
        let mk = |name: &str| ModifierInfo {
            name: name.into(),
            ..ModifierInfo::default()
        };
        let mut list = vec![
            mk("B other"),
            mk("A other"),
            mk("Zelda"),
            mk("zelda trainer"),
        ];
        sort_by_relevance(&mut list, "zelda");
        let names: Vec<_> = list.iter().map(|m| m.name.as_str()).collect();
        assert_eq!(names, ["Zelda", "zelda trainer", "B other", "A other"]);
    }
}
