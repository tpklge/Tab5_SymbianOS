// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "camera_format.hpp"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "media.hpp"
#include <deque>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
namespace {
std::mutex audioLock;
esp_codec_dev_handle_t speaker = nullptr, mic = nullptr;
std::string diagnostic = "not initialized";
bool initialized = false, speakerOpen = false, micOpen = false;
int currentVolume = -1;
bool initAudio() {
  if (initialized)
    return true;
  i2s_std_config_t config{};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000);
  config.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO);
  config.gpio_cfg.mclk = BSP_I2S_MCLK;
  config.gpio_cfg.bclk = BSP_I2S_SCLK;
  config.gpio_cfg.ws = BSP_I2S_LCLK;
  config.gpio_cfg.dout = BSP_I2S_DOUT;
  config.gpio_cfg.din = BSP_I2S_DSIN;
  auto result = bsp_audio_init(&config);
  if (result != ESP_OK) {
    diagnostic = "I2S init: " + std::to_string(result);
    return false;
  }
  initialized = true;
  return true;
}
bool output(const int16_t *pcm, size_t count, unsigned rate, unsigned channels,
            int volume) {
  std::lock_guard<std::mutex> lock(audioLock);
  if (!pcm || !count || rate < 8000 || rate > 48000 || channels < 1 ||
      channels > 2 || !initAudio())
    return false;
  if (!speaker) {
    speaker = bsp_audio_codec_speaker_init();
    if (!speaker) {
      diagnostic = "ES8388 unavailable";
      return false;
    }
  }
  if (!speakerOpen) {
    esp_codec_dev_sample_info_t info{};
    info.sample_rate = 48000;
    info.bits_per_sample = 16;
    info.channel = 2;
    info.channel_mask = 3;
    int result = esp_codec_dev_open(speaker, &info);
    if (result != ESP_CODEC_DEV_OK) {
      diagnostic = "ES8388 open: " + std::to_string(result);
      return false;
    }
  }
  speakerOpen = true;
  if (currentVolume != volume) {
    if (bsp_feature_enable(BSP_FEATURE_SPEAKER, true) != ESP_OK ||
        esp_codec_dev_set_out_mute(speaker, false) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_out_vol(speaker, volume) != ESP_CODEC_DEV_OK) {
      diagnostic = "speaker enable/unmute/volume failed";
      return false;
    }
    currentVolume = volume;
  }
  size_t frames = count / channels, outFrames = frames * 48000 / rate;
  static std::vector<int16_t> stereo;
  stereo.resize(outFrames * 2);
  for (size_t i = 0; i < outFrames; ++i) {
    auto source = std::min(frames - 1, i * rate / 48000);
    stereo[i * 2] = pcm[source * channels];
    stereo[i * 2 + 1] = pcm[source * channels + (channels == 2 ? 1 : 0)];
  }
  int result = esp_codec_dev_write(speaker, stereo.data(), stereo.size() * 2);
  diagnostic = result == ESP_CODEC_DEV_OK
                   ? "ok"
                   : "I2S write: " + std::to_string(result);
  return result == ESP_CODEC_DEV_OK;
}
// Bounded PCM queue absorbs SD/decode stalls without holding the UI/state lock.
// Allocated lazily: no additional audio task is created during system startup.
class AudioQueue {
  struct Packet {
    std::vector<int16_t> pcm;
    unsigned rate, channels;
  };
  std::mutex lock_;
  std::deque<Packet> queue_;
  tab5::Executor worker_{32768, 5};
  std::atomic<unsigned> generation_{0};
  std::atomic<bool> paused_{false};
  tab5::Media &media_;
  bool failed_ = false, inFlight_ = false;

public:
  explicit AudioQueue(tab5::Media &media) : media_(media) {
    worker_.submit([this] {
      for (;;) {
        Packet packet;
        unsigned generation;
        {
          std::lock_guard<std::mutex> lock(lock_);
          if (!paused_ && !queue_.empty()) {
            packet = std::move(queue_.front());
            queue_.pop_front();
            inFlight_ = true;
          }
          generation = generation_;
        }
        if (packet.pcm.empty()) {
          tab5::workerWait(std::chrono::milliseconds(5));
          continue;
        }
        bool ok = output(packet.pcm.data(), packet.pcm.size(), packet.rate,
                         packet.channels, media_.volume());
        {
          std::lock_guard<std::mutex> lock(lock_);
          inFlight_ = false;
          if (!ok && generation == generation_) {
            failed_ = true;
            queue_.clear();
          }
        }
      }
    });
  }
  bool write(const int16_t *p, size_t n, unsigned r, unsigned c,
             const std::atomic<unsigned> &epoch, unsigned expected,
             uint64_t playback) {
    if (!worker_.joinable())
      return false;
    unsigned generation = generation_;
    for (;;) {
      {
        std::lock_guard<std::mutex> lock(lock_);
        if (generation != generation_ || epoch.load() != expected ||
            !media_.playbackGeneration(playback))
          return true;
        if (failed_)
          return false;
        if (queue_.size() < 12) {
          queue_.push_back({std::vector<int16_t>(p, p + n), r, c});
          return true;
        }
      }
      tab5::workerWait(std::chrono::milliseconds(5));
    }
  }
  void flush() {
    std::lock_guard<std::mutex> lock(lock_);
    ++generation_;
    queue_.clear();
    failed_ = false;
    paused_ = false;
  }
  void pause(bool on) { paused_ = on; }
  bool drained() {
    std::lock_guard<std::mutex> lock(lock_);
    return queue_.empty() && !inFlight_;
  }
};
std::atomic<AudioQueue *> audioQueue{nullptr};
std::atomic<unsigned> audioGeneration{0};
std::once_flag audioQueueOnce;
tab5::Media *mediaControl = nullptr;
bool input(std::vector<int16_t> &samples) {
  std::lock_guard<std::mutex> lock(audioLock);
  if (!initAudio())
    return false;
  if (!mic) {
    mic = bsp_audio_codec_microphone_init();
    if (!mic) {
      diagnostic = "ES7210 unavailable";
      return false;
    }
  }
  if (!micOpen) {
    esp_codec_dev_sample_info_t info{};
    info.sample_rate = 48000;
    info.bits_per_sample = 16;
    info.channel = 2;
    info.channel_mask = 3;
    int result = esp_codec_dev_open(mic, &info);
    if (result != ESP_CODEC_DEV_OK) {
      diagnostic = "microphone open failed";
      return false;
    }
    if (esp_codec_dev_set_in_gain(mic, 30) != ESP_CODEC_DEV_OK) {
      diagnostic = "microphone gain failed";
      return false;
    }
    micOpen = true;
  }
  int16_t raw[3072];
  int result = esp_codec_dev_read(mic, raw, sizeof(raw));
  if (result != ESP_CODEC_DEV_OK) {
    diagnostic = "microphone read failed";
    return false;
  }
  samples.resize(512);
  for (size_t i = 0; i < samples.size(); ++i)
    samples[i] = (int(raw[i * 6]) + raw[i * 6 + 1]) / 2;
  return true;
}
std::mutex cameraLock;
struct CameraState {
  int fd = -1;
  bool started = false, streaming = false;
  void *buffers[2] = {};
  size_t lengths[2] = {};
  v4l2_format format{};
} cam;
std::string cameraDiagnostic = "Câmera ainda não iniciada";
void closeCameraUnlocked() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (cam.streaming)
    ioctl(cam.fd, VIDIOC_STREAMOFF, &type);
  for (unsigned i = 0; i < 2; ++i)
    if (cam.buffers[i]) {
      munmap(cam.buffers[i], cam.lengths[i]);
      cam.buffers[i] = nullptr;
    }
  if (cam.fd >= 0)
    close(cam.fd);
  cam.fd = -1;
  cam.streaming = false;
}
void closeCamera() {
  std::lock_guard<std::mutex> lock(cameraLock);
  closeCameraUnlocked();
}
bool capture(tab5::Picture &pic) {
  std::lock_guard<std::mutex> lock(cameraLock);
  auto fail = [&](const char *stage) {
    cameraDiagnostic = std::string(stage) + " • erro " + std::to_string(errno);
    closeCameraUnlocked();
    return false;
  };
  if (!cam.started) {
    bsp_camera_cfg_t config{};
    auto rc = bsp_camera_start(&config);
    if (rc != ESP_OK) {
      cameraDiagnostic = "Inicialização do sensor: " + std::to_string(rc);
      return false;
    }
    cam.started = true;
  }
  if (cam.fd < 0) {
    cam.fd = open(BSP_CAMERA_DEVICE, O_RDWR | O_NONBLOCK);
    if (cam.fd < 0) {
      cam.started = false;
      esp_video_deinit();
      return fail("Abrir /dev/video0 (sensor não detectado)");
    }
    timeval timeout{1, 0};
    if (ioctl(cam.fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0)
      return fail("Timeout da câmera");
    const char *stage = "Consultar formato";
    if (!tab5::cameraCaptureFormat(
            [&](unsigned long op, v4l2_format *format) {
              return ioctl(cam.fd, op, format);
            },
            cam.format, stage))
      return fail(stage);
    v4l2_requestbuffers request{};
    request.count = 2;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cam.fd, VIDIOC_REQBUFS, &request) != 0 || request.count < 2)
      return fail("Reservar buffers");
    for (unsigned i = 0; i < 2; ++i) {
      v4l2_buffer buffer{};
      buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buffer.memory = V4L2_MEMORY_MMAP;
      buffer.index = i;
      if (ioctl(cam.fd, VIDIOC_QUERYBUF, &buffer) != 0)
        return fail("Consultar buffer");
      cam.lengths[i] = buffer.length;
      cam.buffers[i] = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, cam.fd, buffer.m.offset);
      if (cam.buffers[i] == MAP_FAILED) {
        cam.buffers[i] = nullptr;
        return fail("Mapear buffer");
      }
      if (ioctl(cam.fd, VIDIOC_QBUF, &buffer) != 0)
        return fail("Enfileirar buffer");
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam.fd, VIDIOC_STREAMON, &type) != 0)
      return fail("Iniciar captura");
    cam.streaming = true;
  }
  v4l2_buffer buffer{};
  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_MMAP;
  if (ioctl(cam.fd, VIDIOC_DQBUF, &buffer) != 0 || buffer.index >= 2)
    return fail("Receber imagem");
  unsigned width = cam.format.fmt.pix.width, height = cam.format.fmt.pix.height,
           stride =
               std::max<unsigned>(width * 2, cam.format.fmt.pix.bytesperline);
  if (uint64_t(stride) * height > cam.lengths[buffer.index])
    return fail("Tamanho do buffer");
  pic.width = 640;
  pic.height = 360;
  pic.pixels.resize(640 * 360 * 2);
  unsigned cropW = width, cropH = height;
  if (uint64_t(width) * 9 > uint64_t(height) * 16)
    cropW = height * 16 / 9;
  else
    cropH = width * 9 / 16;
  auto *data = static_cast<const char *>(cam.buffers[buffer.index]);
  for (unsigned y = 0; y < 360; ++y)
    for (unsigned x = 0; x < 640; ++x) {
      size_t source = ((height - cropH) / 2 + y * cropH / 360) * stride +
                      ((width - cropW) / 2 + x * cropW / 640) * 2,
             target = (y * 640 + x) * 2;
      pic.pixels[target] = data[source];
      pic.pixels[target + 1] = data[source + 1];
    }
  if (ioctl(cam.fd, VIDIOC_QBUF, &buffer) != 0)
    return fail("Devolver buffer");
  cameraDiagnostic = "Câmera ativa";
  return true;
}
} // namespace
std::string tab5_audio_diagnostic();
void tab5_setup_media(tab5::Media &media) {
  mediaControl = &media;
  media.device.writeTagged = [&media](const int16_t *p, size_t n, unsigned r,
                                      unsigned c, uint64_t playback) {
    auto generation = audioGeneration.load();
    std::call_once(audioQueueOnce,
                   [&media] { audioQueue.store(new AudioQueue(media)); });
    if (generation != audioGeneration.load())
      return true;
    return audioQueue.load()->write(p, n, r, c, audioGeneration, generation,
                                    playback);
  };
  media.device.flush = [] {
    ++audioGeneration;
    if (auto *q = audioQueue.load())
      q->flush();
  };
  media.device.pause = [](bool on) {
    if (auto *q = audioQueue.load())
      q->pause(on);
  };
  media.device.drained = [] {
    auto *q = audioQueue.load();
    return !q || q->drained();
  };
  media.device.read = input;
  media.device.capture = capture;
  media.device.closeCamera = closeCamera;
  media.device.cameraDiagnostic = [] {
    std::lock_guard<std::mutex> lock(cameraLock);
    return cameraDiagnostic;
  };
  media.device.diagnostic = tab5_audio_diagnostic;
}
std::string tab5_audio_diagnostic() {
  std::lock_guard<std::mutex> lock(audioLock);
  return diagnostic;
}
void tab5_signal(bool alarm) {
  // Fixed stereo 48 kHz output matches the shared I2S bus; the codec remains
  // open.
  int16_t tone[4800];
  for (unsigned block = 0; block < (alarm ? 12u : 4u); ++block) {
    for (unsigned i = 0; i < 4800; ++i) {
      unsigned choice=alarm && mediaControl?mediaControl->alertTone():0;
      unsigned period=choice==1?72:choice==2?(block%2?40:60):48;
      unsigned phase = (i + block * 4800) % period;
      int envelope = (i < 160 ? i : 4800 - i < 160 ? 4800 - i : 160);
      int sample=choice==1?(int(phase<period/2?phase:period-phase)*12000/int(period/2)-6000):(phase<period/2?9000:-9000);
      if(choice==2 && block%4==3)sample=0;
      tone[i] = sample * envelope / 160;
    }
    if (!output(tone, 4800, 48000, 1,
                mediaControl ? mediaControl->volume() : 60))
      break;
  }
  ESP_LOGI("audio", "notification result: %s", tab5_audio_diagnostic().c_str());
}
