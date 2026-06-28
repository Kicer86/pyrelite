
#include <cstdio>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QMessageLogContext>
#include <QQmlApplicationEngine>
#include <QVariantMap>

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

namespace
{
    QtMessageHandler previousMessageHandler = nullptr;

    bool isNoisyAudioStartupMessage(QtMsgType type,
                                    const QMessageLogContext &context,
                                    const QString &message)
    {
        const QString category = QString::fromUtf8(context.category ? context.category : "");
        if ((type == QtInfoMsg || type == QtWarningMsg)
            && category == QStringLiteral("qt.multimedia.ffmpeg")
            && message.startsWith(QStringLiteral("Using Qt multimedia with FFmpeg version")))
            return true;

        if (type == QtWarningMsg
            && message.startsWith(QStringLiteral("spaVisitChoice: parse error"))
            && message.contains(QStringLiteral("Spa:Enum:MediaSubtype:iec958")))
            return true;

        return false;
    }

    void filteredMessageHandler(QtMsgType type,
                                const QMessageLogContext &context,
                                const QString &message)
    {
        if (isNoisyAudioStartupMessage(type, context, message))
            return;

        if (previousMessageHandler)
        {
            previousMessageHandler(type, context, message);
            return;
        }

        const QByteArray formatted = qFormatLogMessage(type, context, message).toLocal8Bit();
        std::fprintf(stderr, "%s\n", formatted.constData());
    }

    void quietNoisyAudioBackendStartupLogs()
    {
#ifndef Q_OS_WASM
        if (!qEnvironmentVariableIsSet("PIPEWIRE_DEBUG"))
            qputenv("PIPEWIRE_DEBUG", "1");
#endif
        previousMessageHandler = qInstallMessageHandler(filteredMessageHandler);
    }
}

int main(int argc, char *argv[])
{
    quietNoisyAudioBackendStartupLogs();

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Pyrelite"));
    parser.addHelpOption();
    const QCommandLineOption previewOption(
        QStringLiteral("preview"),
        QStringLiteral("Fly freely over the generated world without running gameplay."));
    parser.addOption(previewOption);
    parser.process(app);

    QQmlApplicationEngine engine;
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("previewMode"), parser.isSet(previewOption));
    engine.setInitialProperties(initialProperties);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("PyreliteApp", "Main");

#ifdef Q_OS_WASM
    // On the web, keyboard events only reach Qt while the rendering <canvas> has
    // DOM focus, and that focus is easily lost (and a plain container click does
    // not restore it). Make the canvas focusable and (re)focus it on any pointer
    // interaction so keyboard control keeps working.
    EM_ASM({
        var focusCanvas = function() {
            var screen = document.querySelector('#screen');
            var canvas = screen ? screen.querySelector('canvas') : null;
            if (canvas) {
                canvas.setAttribute('tabindex', '0');
                canvas.style.outline = 'none';
                canvas.focus();
            }
        };
        window.addEventListener('pointerdown', focusCanvas, true);
        setTimeout(focusCanvas, 500);
    });
#endif

    return app.exec();
}
