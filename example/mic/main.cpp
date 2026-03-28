#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <lowwi.hpp>

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mutex              audio_mutex;
static std::vector<float>      audio_buffer;
static std::atomic<bool>       running{true};

void on_capture(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    auto* samples = static_cast<const float*>(input);
    std::lock_guard lock(audio_mutex);
    audio_buffer.insert(audio_buffer.end(), samples, samples + frame_count);
}

void on_wakeword(CLFML::LOWWI::Lowwi_ctx_t ctx, std::shared_ptr<void>) {
    std::cout << "🗣️ Detected: " << ctx.phrase
              << " (" << ctx.confidence << ")\n";
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <model.onnx> [model2.onnx ...]\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::filesystem::path p{argv[i]};
        if (!std::filesystem::exists(p)) {
            std::cerr << "File not found: " << p.string() << "\n";
            return 1;
        }
        if (p.extension() != ".onnx") {
            std::cerr << "Not an .onnx file: " << p.string() << "\n";
            return 1;
        }
    }

    std::signal(SIGINT, [](int) { running = false; });

    CLFML::LOWWI::Lowwi ww;

    for (int i = 1; i < argc; ++i) {
        auto abs_path = std::filesystem::absolute(argv[i]);
        CLFML::LOWWI::Lowwi_word_t word;
        word.phrase     = abs_path.string();
        word.model_path = abs_path;
        word.cbfunc     = on_wakeword;
        word.debug           = true;
        word.min_activations = 2;
        ww.add_wakeword(word);
    }

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate       = 16000;
    config.dataCallback     = on_capture;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize capture device\n";
        return 1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        std::cerr << "Failed to start capture device\n";
        ma_device_uninit(&device);
        return 1;
    }

    std::cout << "🎙️ Capture device: " << device.capture.name << "\n";
    std::cout << "🎙️ Listening...\n";

    std::vector<float> chunk;
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        {
            std::lock_guard lock(audio_mutex);
            if (audio_buffer.empty()) continue;
            chunk.swap(audio_buffer);
        }
        ww.run(chunk);
        chunk.clear();
    }

    ma_device_uninit(&device);
}
