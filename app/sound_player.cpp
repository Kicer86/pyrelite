#include "sound_player.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QFile>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <QDebug>

#include <miniaudio.h>

namespace
{
    constexpr int kSampleRate = 48000;
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

// Lock-protected accumulation buffer for the active sound effects. play()
// enqueues freshly rendered PCM from the main thread; read() is pulled by the
// miniaudio device on its own audio thread, draining mono int16 samples and
// zero-filling whatever silence the callback still asks for.
class SoundPlayer::Mixer
{
public:
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

    void read(qint16 *output, int sampleCount)
    {
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
    }

    // ma_device_data_proc: pulled on miniaudio's audio thread.
    static void dataCallback(ma_device *device, void *output, const void *, ma_uint32 frameCount)
    {
        auto *mixer = static_cast<Mixer *>(device->pUserData);
        mixer->read(static_cast<qint16 *>(output), static_cast<int>(frameCount));
    }

private:
    QMutex m_mutex;
    QVector<int> m_samples;
    int m_readOffset = 0;
};

// Owns the miniaudio playback device. Kept out of the header so the ~4 MB
// miniaudio amalgamation only has to be parsed by this translation unit.
struct SoundPlayer::Backend
{
    ma_device device{};
    bool started = false;

    ~Backend()
    {
        if (started)
            ma_device_uninit(&device);
    }
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
    if (m_mixer)
        m_mixer->enqueue(data);
}

void SoundPlayer::ensureStarted()
{
    if (m_backend)
        return;

    auto mixer = std::make_unique<Mixer>();
    auto backend = std::make_unique<Backend>();

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 1;
    config.sampleRate = kSampleRate;
    config.dataCallback = &Mixer::dataCallback;
    config.pUserData = mixer.get();

    if (ma_device_init(nullptr, &config, &backend->device) != MA_SUCCESS)
    {
        qWarning() << "Could not initialise the audio output device";
        return;
    }

    if (ma_device_start(&backend->device) != MA_SUCCESS)
    {
        qWarning() << "Could not start the audio output device";
        ma_device_uninit(&backend->device);
        return;
    }
    backend->started = true;

    m_mixer = std::move(mixer);
    m_backend = std::move(backend);
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
