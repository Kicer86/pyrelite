#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>

class SoundPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SoundPlayer(QObject *parent = nullptr);
    ~SoundPlayer() override;

    Q_INVOKABLE void preload(const QUrl &source);
    Q_INVOKABLE void warmUp();
    Q_INVOKABLE void play(const QUrl &source);

private:
    class Mixer;
    struct Backend;

    void ensureStarted();
    QByteArray soundData(const QUrl &source);

    QHash<QString, QByteArray> m_cache;
    std::unique_ptr<Mixer> m_mixer;
    std::unique_ptr<Backend> m_backend;
};
