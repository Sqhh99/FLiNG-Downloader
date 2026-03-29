#include <QGuiApplication>

#include <benchmark/benchmark.h>

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QGuiApplication::setApplicationName("FLiNG Downloader Benchmarks");
    QGuiApplication app(argc, argv);

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
