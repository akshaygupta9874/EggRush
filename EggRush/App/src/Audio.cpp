#include "Audio.hpp"

#include <QDir>
#include <QFile>
#include <QSoundEffect>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace er {

namespace {

constexpr int kRate = 44100;
constexpr double kTwoPi = 6.283185307179586;

enum class Wave { Sine, Tri, Square, Saw };

struct Buf {
  std::vector<float> s;
  bool circular = false;
  explicit Buf(double seconds, bool circ = false)
      : s(static_cast<size_t>(seconds * kRate), 0.f), circular(circ) {}

  void mix(size_t i, float v) {
    if (circular) {
      s[i % s.size()] += v;
    } else if (i < s.size()) {
      s[i] += v;
    }
  }

  // Frequency glides f0 -> f1 over `dur`; amplitude: short attack then power-curve decay.
  void tone(double start, double dur, double f0, double f1, float vol, Wave w = Wave::Sine,
            double attack = 0.004, double curve = 3.0) {
    const size_t n = static_cast<size_t>(dur * kRate);
    const size_t o = static_cast<size_t>(start * kRate);
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i) {
      const double t = static_cast<double>(i) / static_cast<double>(n);
      const double f = f0 + (f1 - f0) * t;
      phase += kTwoPi * f / kRate;
      const double ph = std::fmod(phase, kTwoPi) / kTwoPi;
      double v = 0.0;
      switch (w) {
        case Wave::Sine: v = std::sin(phase); break;
        case Wave::Tri: v = 4.0 * std::fabs(ph - 0.5) - 1.0; break;
        case Wave::Square: v = ph < 0.5 ? 0.6 : -0.6; break;
        case Wave::Saw: v = 2.0 * ph - 1.0; break;
      }
      const double sec = static_cast<double>(i) / kRate;
      const double env = (sec < attack ? sec / attack : 1.0) * std::pow(1.0 - t, curve);
      mix(o + i, static_cast<float>(v * env) * vol);
    }
  }

  void noise(double start, double dur, float vol, double lowpass = 0.3, double curve = 3.0) {
    const size_t n = static_cast<size_t>(dur * kRate);
    const size_t o = static_cast<size_t>(start * kRate);
    uint32_t seed = 12345u + static_cast<uint32_t>(o);
    double y = 0.0;
    for (size_t i = 0; i < n; ++i) {
      seed = seed * 1664525u + 1013904223u;
      const double r = static_cast<double>(seed >> 8) / 8388608.0 - 1.0;
      y += lowpass * (r - y);
      const double t = static_cast<double>(i) / static_cast<double>(n);
      mix(o + i, static_cast<float>(y * std::pow(1.0 - t, curve)) * vol);
    }
  }

  QByteArray wav() const {
    float peak = 0.001f;
    for (float v : s) peak = std::max(peak, std::fabs(v));
    const float g = peak > 0.95f ? 0.95f / peak : 1.f;
    QByteArray pcm;
    pcm.resize(static_cast<qsizetype>(s.size() * 2));
    auto* d = reinterpret_cast<int16_t*>(pcm.data());
    for (size_t i = 0; i < s.size(); ++i) d[i] = static_cast<int16_t>(std::clamp(s[i] * g, -1.f, 1.f) * 32767.f);
    QByteArray out;
    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.append(static_cast<char>((v >> (8 * i)) & 0xff)); };
    auto le16 = [&](uint16_t v) { for (int i = 0; i < 2; ++i) out.append(static_cast<char>((v >> (8 * i)) & 0xff)); };
    out.append("RIFF");
    le32(static_cast<uint32_t>(36 + pcm.size()));
    out.append("WAVEfmt ");
    le32(16);
    le16(1);
    le16(1);
    le32(kRate);
    le32(kRate * 2);
    le16(2);
    le16(16);
    out.append("data");
    le32(static_cast<uint32_t>(pcm.size()));
    out.append(pcm);
    return out;
  }
};

constexpr double kScale[8] = {523.25, 587.33, 659.25, 783.99, 880.00, 1046.50, 1174.66, 1318.51};

Buf makeSfx(Sfx s, int variant) {
  switch (s) {
    case Sfx::UiMove: {
      Buf b(0.06);
      b.tone(0, 0.05, 720, 700, 0.35f, Wave::Tri, 0.002, 2.5);
      return b;
    }
    case Sfx::UiConfirm: {
      Buf b(0.18);
      b.tone(0, 0.09, 600, 900, 0.4f, Wave::Tri, 0.003, 2.0);
      b.tone(0.07, 0.1, 900, 1200, 0.3f, Wave::Sine, 0.003, 2.5);
      return b;
    }
    case Sfx::UiBack: {
      Buf b(0.14);
      b.tone(0, 0.11, 560, 340, 0.4f, Wave::Tri, 0.003, 2.0);
      return b;
    }
    case Sfx::Telegraph: {
      Buf b(0.12);
      b.noise(0, 0.05, 0.12f, 0.15, 2.0);
      b.tone(0, 0.08, 300, 240, 0.12f, Wave::Tri, 0.004, 2.5);
      return b;
    }
    case Sfx::Catch: {
      const double f = kScale[std::clamp(variant, 0, 7)];
      Buf b(0.3);
      b.tone(0, 0.22, f, f, 0.5f, Wave::Tri, 0.003, 3.2);
      b.tone(0, 0.12, f * 2, f * 2, 0.18f, Wave::Sine, 0.002, 3.5);
      b.tone(0, 0.07, 190, 110, 0.5f, Wave::Sine, 0.002, 2.0);
      return b;
    }
    case Sfx::Perfect: {
      Buf b(0.5);
      b.tone(0, 0.3, 1318.5, 1318.5, 0.4f, Wave::Sine, 0.003, 3.0);
      b.tone(0.06, 0.38, 1760, 1760, 0.35f, Wave::Sine, 0.003, 3.0);
      b.tone(0.0, 0.2, 2637, 2637, 0.12f, Wave::Sine, 0.002, 4.0);
      return b;
    }
    case Sfx::Clutch: {
      Buf b(0.5);
      b.tone(0, 0.2, 380, 1900, 0.28f, Wave::Saw, 0.003, 1.6);
      b.tone(0.16, 0.34, 1568, 1568, 0.4f, Wave::Sine, 0.003, 2.8);
      b.tone(0.2, 0.3, 2093, 2093, 0.3f, Wave::Sine, 0.003, 3.0);
      return b;
    }
    case Sfx::Golden: {
      Buf b(0.8);
      const double n[4] = {1046.5, 1318.5, 1568.0, 2093.0};
      for (int i = 0; i < 4; ++i) b.tone(0.06 * i, 0.45, n[i], n[i], 0.34f, Wave::Sine, 0.003, 2.8);
      b.tone(0.2, 0.5, 3136, 3136, 0.1f, Wave::Sine, 0.003, 3.0);
      return b;
    }
    case Sfx::Rim: {
      Buf b(0.14);
      b.noise(0, 0.04, 0.3f, 0.5, 2.0);
      b.tone(0, 0.1, 900, 700, 0.3f, Wave::Square, 0.002, 3.0);
      return b;
    }
    case Sfx::Miss: {
      Buf b(0.35);
      b.tone(0, 0.26, 170, 55, 0.7f, Wave::Sine, 0.002, 2.0);
      b.noise(0, 0.12, 0.35f, 0.25, 2.5);
      return b;
    }
    case Sfx::MissHeavy: {
      Buf b(0.5);
      b.tone(0, 0.4, 120, 38, 0.85f, Wave::Sine, 0.002, 1.8);
      b.noise(0, 0.2, 0.5f, 0.2, 2.0);
      return b;
    }
    case Sfx::HeartLost: {
      Buf b(0.5);
      b.tone(0, 0.22, 440, 392, 0.4f, Wave::Tri, 0.004, 2.0);
      b.tone(0.16, 0.3, 349, 262, 0.4f, Wave::Tri, 0.004, 2.0);
      return b;
    }
    case Sfx::HeartGained: {
      Buf b(0.5);
      b.tone(0, 0.2, 523, 523, 0.34f, Wave::Sine, 0.004, 2.2);
      b.tone(0.12, 0.3, 784, 784, 0.34f, Wave::Sine, 0.004, 2.5);
      return b;
    }
    case Sfx::Wave: {
      Buf b(0.8);
      const double n[3] = {523.25, 659.25, 783.99};
      for (int i = 0; i < 3; ++i) b.tone(0.11 * i, 0.4, n[i], n[i], 0.32f, Wave::Tri, 0.004, 2.4);
      return b;
    }
    case Sfx::Objective: {
      Buf b(0.7);
      b.tone(0, 0.25, 784, 784, 0.4f, Wave::Sine, 0.003, 2.5);
      b.tone(0.14, 0.5, 1046.5, 1046.5, 0.4f, Wave::Sine, 0.003, 2.5);
      return b;
    }
    case Sfx::LevelClear: {
      Buf b(1.8);
      const double n[4] = {523.25, 659.25, 783.99, 1046.5};
      for (int i = 0; i < 4; ++i) b.tone(0.16 * i, 0.5, n[i], n[i], 0.3f, Wave::Tri, 0.004, 2.0);
      b.tone(0.64, 1.1, 1046.5, 1046.5, 0.32f, Wave::Sine, 0.01, 1.6);
      b.tone(0.64, 1.1, 523.25, 523.25, 0.2f, Wave::Sine, 0.01, 1.6);
      return b;
    }
    case Sfx::GameOver: {
      Buf b(1.6);
      const double n[3] = {392.0, 329.63, 261.63};
      for (int i = 0; i < 3; ++i) b.tone(0.3 * i, 0.6, n[i], n[i] * 0.97, 0.34f, Wave::Tri, 0.01, 1.6);
      b.tone(0.9, 0.7, 130.8, 110, 0.4f, Wave::Sine, 0.01, 1.4);
      return b;
    }
    case Sfx::Count: break;
  }
  return Buf(0.01);
}

// A gentle 16-beat loop: C - Am - F - G, triangle arpeggio over a sine bass.
Buf makeMusic() {
  const double beat = 60.0 / 104.0;
  Buf b(16 * beat, true);
  const double roots[4] = {130.81, 110.00, 87.31, 98.00};
  const double chord[4][3] = {{261.63, 329.63, 392.0}, {220.0, 261.63, 329.63}, {174.61, 220.0, 261.63}, {196.0, 246.94, 293.66}};
  for (int c = 0; c < 4; ++c) {
    const double t0 = c * 4 * beat;
    b.tone(t0, 4 * beat * 0.95, roots[c], roots[c], 0.28f, Wave::Sine, 0.03, 0.6);
    for (int i = 0; i < 8; ++i) {
      const double f = chord[c][i % 3] * (i % 4 == 3 ? 2.0 : 1.0);
      b.tone(t0 + i * beat * 0.5, beat * 0.9, f, f, 0.16f, Wave::Tri, 0.004, 2.2);
    }
    for (int k = 0; k < 3; ++k) b.tone(t0, 4 * beat, chord[c][k] * 0.5, chord[c][k] * 0.5, 0.06f, Wave::Sine, 0.2, 0.4);
  }
  return b;
}

constexpr int kVersion = 1;

}  // namespace

AudioEngine::AudioEngine(const QString& cacheDir, bool enabled, QObject* parent)
    : QObject(parent), enabled_(enabled) {
  if (enabled_) build(cacheDir);
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::build(const QString& cacheDir) {
  const QString dir = cacheDir + QStringLiteral("/sfx-v%1").arg(kVersion);
  if (!QDir().mkpath(dir)) {
    enabled_ = false;
    return;
  }
  auto ensure = [&](const QString& name, const Buf& buf) {
    const QString path = dir + "/" + name + ".wav";
    if (!QFile::exists(path)) {
      QFile f(path);
      if (!f.open(QIODevice::WriteOnly)) return QString();
      f.write(buf.wav());
    }
    return path;
  };
  auto load = [&](int key, const QString& path, int copies) {
    if (path.isEmpty()) return;
    Voice& v = voices_[key];
    for (int i = 0; i < copies; ++i) {
      auto fx = std::make_unique<QSoundEffect>();
      fx->setSource(QUrl::fromLocalFile(path));
      v.pool.push_back(std::move(fx));
    }
  };
  for (int s = 0; s < static_cast<int>(Sfx::Count); ++s) {
    const Sfx sfx = static_cast<Sfx>(s);
    const int variants = sfx == Sfx::Catch ? 8 : 1;
    for (int v = 0; v < variants; ++v) {
      load(s * 16 + v, ensure(QStringLiteral("sfx%1_%2").arg(s).arg(v), makeSfx(sfx, v)), sfx == Sfx::Catch ? 2 : 2);
    }
  }
  const QString mp = ensure(QStringLiteral("music"), makeMusic());
  if (!mp.isEmpty()) {
    music_fx_ = std::make_unique<QSoundEffect>();
    music_fx_->setSource(QUrl::fromLocalFile(mp));
    music_fx_->setLoopCount(QSoundEffect::Infinite);
  }
  applyVolumes();
}

void AudioEngine::setVolumes(float master, float sfx, float music, bool muted) {
  master_ = master;
  sfx_ = sfx;
  music_ = music;
  muted_ = muted;
  applyVolumes();
}

void AudioEngine::applyVolumes() {
  if (!enabled_) return;
  const float sv = muted_ ? 0.f : master_ * sfx_;
  for (auto& kv : voices_) {
    for (auto& fx : kv.second.pool) fx->setVolume(sv);
  }
  if (music_fx_) music_fx_->setVolume(muted_ ? 0.f : master_ * music_ * (ducked_ ? 0.35f : 0.6f));
}

void AudioEngine::play(Sfx s, int variant) {
  if (!enabled_ || muted_) return;
  const int v = s == Sfx::Catch ? std::clamp(variant, 0, 7) : 0;
  auto it = voices_.find(static_cast<int>(s) * 16 + v);
  if (it == voices_.end() || it->second.pool.empty()) return;
  Voice& voice = it->second;
  QSoundEffect* fx = voice.pool[voice.next++ % voice.pool.size()].get();
  fx->play();
}

void AudioEngine::startMusic() {
  if (enabled_ && music_fx_ && !music_fx_->isPlaying()) music_fx_->play();
}

void AudioEngine::stopMusic() {
  if (music_fx_) music_fx_->stop();
}

void AudioEngine::duckMusic(bool ducked) {
  ducked_ = ducked;
  applyVolumes();
}

}  // namespace er
