
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariantMap>

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

int main(int argc, char *argv[])
{
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
