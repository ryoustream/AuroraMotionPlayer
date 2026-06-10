#include "AuroraApplication.h"
#include "MainWindow.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>

int main(int argc, char* argv[]) {
    AuroraApplication app(argc, argv);

    // High-DPI support
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // Command-line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription("Aurora Motion Player — AI-powered video player");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Video file or URL to open");

    QCommandLineOption benchOpt("benchmark", "Start with benchmark overlay visible");
    QCommandLineOption fullscreenOpt({"f", "fullscreen"}, "Start in fullscreen mode");
    parser.addOption(benchOpt);
    parser.addOption(fullscreenOpt);
    parser.process(app);

    MainWindow window;

    if (parser.isSet(fullscreenOpt))
        window.showFullScreen();
    else
        window.show();

    // Open file/URL if provided
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        const QString& target = args.first();
        if (target.startsWith("http://") || target.startsWith("https://") ||
            target.startsWith("rtmp://") || target.startsWith("rtsp://"))
            window.openURL(target);
        else
            window.openFile(QDir::toNativeSeparators(target));
    }

    return app.exec();
}
