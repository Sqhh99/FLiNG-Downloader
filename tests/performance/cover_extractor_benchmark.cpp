#include <benchmark/benchmark.h>

#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QStringList>

#include "CoverExtractor.h"

namespace {
QStringList sampleImageEntries()
{
    const QDir dir(QStringLiteral(FLING_BENCHMARK_SAMPLE_DIR));
    return dir.entryList(QStringList() << "*.png" << "*.jpg" << "*.jpeg",
                         QDir::Files,
                         QDir::Name);
}

QString sampleImagePathAt(int index)
{
    const QDir dir(QStringLiteral(FLING_BENCHMARK_SAMPLE_DIR));
    const QStringList entries = sampleImageEntries();
    if (index < 0 || index >= entries.size()) {
        return QString();
    }
    return dir.absoluteFilePath(entries.at(index));
}

QString sanitizeBenchmarkName(const QString& fileName)
{
    QString sanitized = fileName;
    for (QChar& ch : sanitized) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }
    return sanitized;
}

void benchmarkSingleImage(benchmark::State& state, const QString& imagePath)
{
    if (imagePath.isEmpty() || !QFileInfo::exists(imagePath)) {
        state.SkipWithError("Sample image not found");
        return;
    }

    (void)CoverExtractor::extractCoverFromLocalImage(imagePath);

    for (auto _ : state) {
        const QPixmap extracted = CoverExtractor::extractCoverFromLocalImage(imagePath);
        benchmark::DoNotOptimize(extracted.cacheKey());
        benchmark::ClobberMemory();
    }

    state.SetLabel(QFileInfo(imagePath).fileName().toStdString());
}

void BM_CoverExtractorAllImages(benchmark::State& state)
{
    const QDir dir(QStringLiteral(FLING_BENCHMARK_SAMPLE_DIR));
    const QStringList entries = sampleImageEntries();
    if (entries.isEmpty()) {
        state.SkipWithError("No sample screenshots found");
        return;
    }

    for (const QString& imageName : entries) {
        (void)CoverExtractor::extractCoverFromLocalImage(dir.absoluteFilePath(imageName));
    }

    for (auto _ : state) {
        for (const QString& imageName : entries) {
            const QPixmap extracted =
                CoverExtractor::extractCoverFromLocalImage(dir.absoluteFilePath(imageName));
            benchmark::DoNotOptimize(extracted.cacheKey());
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * entries.size());
}

const bool kRegisteredBenchmarks = []() {
    const QStringList entries = sampleImageEntries();
    for (int index = 0; index < entries.size(); ++index) {
        const QString benchmarkName = QStringLiteral("CoverExtractor/%1")
                                          .arg(sanitizeBenchmarkName(entries.at(index)));
        const QString imagePath = sampleImagePathAt(index);
        benchmark::RegisterBenchmark(
            benchmarkName.toStdString().c_str(),
            [imagePath](benchmark::State& state) { benchmarkSingleImage(state, imagePath); })
            ->Unit(benchmark::kMillisecond);
    }

    benchmark::RegisterBenchmark("CoverExtractor/all_images", &BM_CoverExtractorAllImages)
        ->Unit(benchmark::kMillisecond);
    return true;
}();
} // namespace
