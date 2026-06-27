#include "sound_player.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QAudio>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QFile>
#include <QMediaDevices>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <QDebug>

namespace
{
    constexpr int kSampleRate = 48000;
    constexpr int kOutputBufferMs = 50;
    constexpr int kAdvertisedSilenceMs = 500;
    constexpr double kPi = 3.14159265358979323846;

    enum class Wave { Sine, Square, Triangle, Saw, Noise };

    struct Event
    {
        double start = 0.0;
        double duration = 0.0;
        Wave wave = Wave::Sine;
        double startHz = 440.0;
        double endHz = 440.0;
        double gain = 1.0;
    };

    struct Definition
    {
        double duration = 0.2;
        double volume = 0.5;
        double attack = 0.005;
        double release = 0.04;
        std::vector<Event> events;
    };

    std::optional<double> parseDouble(const QString &value)
    {
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (ok)
            return parsed;
        return std::nullopt;
    }

    std::optional<Wave> parseWave(const QString &value)
    {
        if (value == QStringLiteral("sine"))
            return Wave::Sine;
        if (value == QStringLiteral("square"))
            return Wave::Square;
        if (value == QStringLiteral("triangle"))
            return Wave::Triangle;
        if (value == QStringLiteral("saw"))
            return Wave::Saw;
        if (value == QStringLiteral("noise"))
            return Wave::Noise;
        return std::nullopt;
    }

    QString urlCacheKey(const QUrl &source)
    {
        if (source.isLocalFile())
            return source.toLocalFile();
        return source.toString();
    }

    QString urlFileName(const QUrl &source)
    {
        if (source.isLocalFile())
            return source.toLocalFile();
        if (source.scheme() == QStringLiteral("qrc"))
            return QStringLiteral(":") + source.path();
        return source.toString();
    }

    double eventEnvelope(const Definition &definition, const Event &event, double elapsed)
    {
        double envelope = 1.0;
        const double attack = std::min(definition.attack, event.duration * 0.5);
        const double release = std::min(definition.release, event.duration * 0.5);
        if (attack > 0.0 && elapsed < attack)
            envelope *= elapsed / attack;
        const double remaining = event.duration - elapsed;
        if (release > 0.0 && remaining < release)
            envelope *= std::max(0.0, remaining / release);
        return envelope;
    }

    double noiseAt(int sample, int eventIndex)
    {
        std::uint32_t value = static_cast<std::uint32_t>(sample) * 747796405u
            + static_cast<std::uint32_t>(eventIndex + 1) * 2891336453u;
        value ^= value >> 16;
        value *= 2246822519u;
        value ^= value >> 13;
        return (static_cast<double>(value & 0xffffu) / 32767.5) - 1.0;
    }

    double waveValue(Wave wave, double phase, int sample, int eventIndex)
    {
        switch (wave)
        {
        case Wave::Square:
            return std::sin(phase) >= 0.0 ? 1.0 : -1.0;
        case Wave::Triangle:
            return std::asin(std::sin(phase)) * 2.0 / kPi;
        case Wave::Saw:
        {
            const double cycle = phase / (2.0 * kPi);
            return 2.0 * (cycle - std::floor(cycle + 0.5));
        }
        case Wave::Noise:
            return noiseAt(sample, eventIndex);
        case Wave::Sine:
            break;
        }
        return std::sin(phase);
    }

    Definition parseDefinition(const QString &text, const QString &name)
    {
        Definition definition;
        const QRegularExpression whitespace(QStringLiteral("\\s+"));
        int lineNumber = 0;

        for (const QString &rawLine : text.split(QLatin1Char('\n')))
        {
            ++lineNumber;
            const QString line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const QStringList parts = line.split(whitespace, Qt::SkipEmptyParts);
            const QString command = parts.value(0);
            if (command == QStringLiteral("duration") && parts.size() == 2)
            {
                if (const auto value = parseDouble(parts[1]))
                    definition.duration = std::max(0.01, *value);
                continue;
            }
            if (command == QStringLiteral("volume") && parts.size() == 2)
            {
                if (const auto value = parseDouble(parts[1]))
                    definition.volume = std::clamp(*value, 0.0, 1.0);
                continue;
            }
            if (command == QStringLiteral("envelope") && parts.size() == 3)
            {
                const auto attack = parseDouble(parts[1]);
                const auto release = parseDouble(parts[2]);
                if (attack && release)
                {
                    definition.attack = std::max(0.0, *attack);
                    definition.release = std::max(0.0, *release);
                }
                continue;
            }
            if (command == QStringLiteral("tone") && parts.size() == 7)
            {
                const auto start = parseDouble(parts[1]);
                const auto duration = parseDouble(parts[2]);
                const auto wave = parseWave(parts[3]);
                const auto startHz = parseDouble(parts[4]);
                const auto endHz = parseDouble(parts[5]);
                const auto gain = parseDouble(parts[6]);
                if (start && duration && wave && startHz && endHz && gain)
                    definition.events.push_back(Event{
                        std::max(0.0, *start),
                        std::max(0.001, *duration),
                        *wave,
                        std::max(1.0, *startHz),
                        std::max(1.0, *endHz),
                        std::max(0.0, *gain)});
                continue;
            }
            if (command == QStringLiteral("noise") && parts.size() == 4)
            {
                const auto start = parseDouble(parts[1]);
                const auto duration = parseDouble(parts[2]);
                const auto gain = parseDouble(parts[3]);
                if (start && duration && gain)
                    definition.events.push_back(Event{
                        std::max(0.0, *start),
                        std::max(0.001, *duration),
                        Wave::Noise,
                        1.0,
                        1.0,
                        std::max(0.0, *gain)});
                continue;
            }

            qWarning() << "Ignoring invalid sfx line" << name << lineNumber << line;
        }

        return definition;
    }

    QByteArray renderDefinition(const Definition &definition)
    {
        const int sampleCount = std::max(1, static_cast<int>(definition.duration * kSampleRate));
        QByteArray data;
        data.resize(sampleCount * static_cast<int>(sizeof(qint16)));
        auto *samples = reinterpret_cast<qint16 *>(data.data());

        for (int i = 0; i < sampleCount; ++i)
        {
            const double time = static_cast<double>(i) / kSampleRate;
            double mixed = 0.0;
            for (std::size_t eventIndex = 0; eventIndex < definition.events.size(); ++eventIndex)
            {
                const Event &event = definition.events[eventIndex];
                if (time < event.start || time >= event.start + event.duration)
                    continue;

                const double elapsed = time - event.start;
                const double sweep = (event.endHz - event.startHz) / event.duration;
                const double cycles = event.startHz * elapsed + 0.5 * sweep * elapsed * elapsed;
                const double phase = 2.0 * kPi * cycles;
                mixed += waveValue(event.wave, phase, i, static_cast<int>(eventIndex))
                    * event.gain * eventEnvelope(definition, event, elapsed);
            }

            mixed = std::clamp(mixed * definition.volume, -1.0, 1.0);
            samples[i] = static_cast<qint16>(mixed * 32767.0);
        }

        return data;
    }
}

class SoundPlayer::MixerDevice : public QIODevice
{
public:
    explicit MixerDevice(QObject *parent = nullptr)
        : QIODevice(parent)
    {
    }

    void enqueue(const QByteArray &data)
    {
        QMutexLocker lock(&m_mutex);

        const auto *input = reinterpret_cast<const qint16 *>(data.constData());
        const int count = data.size() / static_cast<int>(sizeof(qint16));
        if (m_samples.size() - m_readOffset < count)
            m_samples.resize(m_readOffset + count);

        for (int i = 0; i < count; ++i)
            m_samples[m_readOffset + i] += input[i];
    }

    qint64 readData(char *data, qint64 maxSize) override
    {
        if (maxSize < static_cast<qint64>(sizeof(qint16)))
            return 0;

        const qint64 byteCount = maxSize - maxSize % static_cast<qint64>(sizeof(qint16));
        auto *output = reinterpret_cast<qint16 *>(data);
        const int sampleCount = static_cast<int>(byteCount / static_cast<qint64>(sizeof(qint16)));

        QMutexLocker lock(&m_mutex);
        for (int i = 0; i < sampleCount; ++i)
        {
            if (m_readOffset < m_samples.size())
                output[i] = static_cast<qint16>(
                    std::clamp(m_samples[m_readOffset++], -32768, 32767));
            else
                output[i] = 0;
        }

        if (m_readOffset >= m_samples.size())
        {
            m_samples.clear();
            m_readOffset = 0;
        }
        else if (m_readOffset > kSampleRate)
        {
            m_samples.erase(m_samples.begin(), m_samples.begin() + m_readOffset);
            m_readOffset = 0;
        }

        return byteCount;
    }

    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

    qint64 bytesAvailable() const override
    {
        QMutexLocker lock(&m_mutex);
        const qint64 queued = (m_samples.size() - m_readOffset)
            * static_cast<qint64>(sizeof(qint16));
        const qint64 silence = kSampleRate * static_cast<qint64>(sizeof(qint16))
            * kAdvertisedSilenceMs / 1000;
        return queued + silence + QIODevice::bytesAvailable();
    }

private:
    mutable QMutex m_mutex;
    QVector<int> m_samples;
    int m_readOffset = 0;
};

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent)
{
}

SoundPlayer::~SoundPlayer() = default;

void SoundPlayer::preload(const QUrl &source)
{
    soundData(source);
}

void SoundPlayer::warmUp()
{
    ensureStarted();
}

void SoundPlayer::play(const QUrl &source)
{
    const QByteArray data = soundData(source);
    if (data.isEmpty())
        return;

    ensureStarted();
    if (m_device)
        m_device->enqueue(data);
}

void SoundPlayer::ensureStarted()
{
    if (m_sink)
        return;

    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (!output.isNull() && !output.isFormatSupported(format))
    {
        qWarning() << "Audio output does not support the sfx format";
        return;
    }

    m_device = std::make_unique<MixerDevice>();
    m_device->open(QIODevice::ReadOnly);
    if (output.isNull())
        m_sink = std::make_unique<QAudioSink>(format, this);
    else
        m_sink = std::make_unique<QAudioSink>(output, format, this);
    m_sink->setBufferSize(kSampleRate * static_cast<int>(sizeof(qint16))
                          * kOutputBufferMs / 1000);

    connect(m_sink.get(), &QAudioSink::stateChanged, this,
        [this](QAudio::State state)
        {
            if (state == QAudio::StoppedState && m_sink && m_sink->error() != QAudio::NoError)
            {
                qWarning() << "Audio output stopped" << m_sink->error();
                QMetaObject::invokeMethod(this,
                    [this]()
                    {
                        m_sink.reset();
                        m_device.reset();
                    },
                    Qt::QueuedConnection);
            }
        });
    m_sink->start(m_device.get());
}

QByteArray SoundPlayer::soundData(const QUrl &source)
{
    const QString cacheKey = urlCacheKey(source);
    const auto cached = m_cache.constFind(cacheKey);
    if (cached != m_cache.constEnd())
        return cached.value();

    QFile file(urlFileName(source));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Could not open sfx" << source;
        return {};
    }

    const QString text = QString::fromUtf8(file.readAll());
    const QByteArray rendered = renderDefinition(parseDefinition(text, cacheKey));
    m_cache.insert(cacheKey, rendered);
    return rendered;
}
