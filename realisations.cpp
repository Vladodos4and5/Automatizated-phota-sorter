#include "functions.h"
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>

//----------------- Изображение -------------------------

Image::Image(std::string path)
    : path_(std::move(path)) {
    std::replace(path_.begin(), path_.end(), '\\', '/');
}

std::string Image::GetBase64() const {
    return base64_form_;
}

void Image::CreateBase64() {
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        std::cerr << "ERROR: cannot open file: " << path_ << std::endl;
        return;
    }

    std::cout << "Reading file: " << path_ << std::endl;

    std::vector<unsigned char> buffer(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
        );

    base64_form_ = Base64Encode(buffer.data(), buffer.size());
}

void Image::SetDescription(std::string_view description) {
    description_ = description;
}

std::string Image::GetDescription() const {
    return description_;
}

std::string Image::GetPath() const {
    std::cout << path_ << " this is path" << std::endl;
    return path_;
}
//--------------- Изображение end -----------------------


static const std::string base64_chars =  // набор для перевода
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";


// ------------- Текстовый запрос на Gemma:12b ----------------
std::string AskGemma3_12b(const std::string& request) {
    httplib::Client cli("127.0.0.1", 11434);
    cli.set_read_timeout(180, 0);
    cli.set_connection_timeout(360, 0);
    nlohmann::json body;
    body["model"] = "gemma3:12b";
    body["prompt"] = request;
    body["stream"] = false;

    auto res = cli.Post("/api/generate", body.dump(), "application/json");

    if (!res || res->status != 200) {
        return "CRITICAL: OLLAMA NOT RESPONDING";
    }

    auto json = nlohmann::json::parse(res->body);
    return json.value("response", "");
}


std::string AskLLaVAVision(
    std::string_view model,
    std::string_view prompt,
    std::string_view image_base64,
    int port)
{
    httplib::Client cli("localhost", port);

    cli.set_read_timeout(120);
    cli.set_write_timeout(120);

    nlohmann::json body;
    body["model"]  = model;
    body["prompt"] = prompt;
    body["stream"] = false;
    body["images"] = { image_base64 };

    auto res = cli.Post("/api/generate", body.dump(), "application/json");

    if (!res || res->status != 200)
        return "OLLAMA ERROR";

    auto json = nlohmann::json::parse(res->body);
    return json.value("response", "");
}


std::string Base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);

    for (size_t i = 0; i < len; i += 3) {
        int val = 0;
        int count = 0;

        for (size_t j = i; j < i + 3; ++j) {
            val <<= 8;
            if (j < len) {
                val |= data[j];
                ++count;
            }
        }

        for (int j = 0; j < 4; ++j) {
            if (j <= count) {
                out.push_back(base64_chars[(val >> (18 - 6 * j)) & 0x3F]);
            } else {
                out.push_back('=');
            }
        }
    }

    return out;
}


std::string ImageToBase64(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};

    std::vector<unsigned char> buffer(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
        );

    return Base64Encode(buffer.data(), buffer.size());
}


std::vector<Image> LoadImagesFromDirectory(const std::string& directory_path) {
    std::vector<Image> images;

    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        return images; // пустой вектор — корректное поведение
    }

    for (const auto& entry : fs::directory_iterator(directory_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".jpg" || ext == ".jpeg" ||
            ext == ".png" || ext == ".webp" ||
            ext == ".bmp") {

            images.emplace_back(entry.path().string());
        }
    }

    return images;
}

void CreateBase64ForVector(std::vector<Image>& images) {
    for (auto& image : images) {
        image.CreateBase64();
    }
}

void DescribeImagesFromVector(
    std::vector<Image>& images,
    std::string_view model,
    std::string_view prompt,
    int port
    ) {
    for (auto& img : images) {
        const std::string& base64 = img.GetBase64();

        if (base64.empty()) {
            std::cerr << "Critical::NO_BASE64_FORM\n";
            continue;
        }

        if (!img.GetDescription().empty()) {
            continue;
        }

        std::string response =
            AskLLaVAVision(model, prompt, base64, port);

        img.SetDescription(response);
    }
}

std::string AskLLaVAText(
    std::string_view model,
    std::string_view prompt,
    int port)
{
    httplib::Client cli("localhost", port);

    cli.set_read_timeout(120);
    cli.set_write_timeout(120);

    nlohmann::json body;
    body["model"]  = model;
    body["prompt"] = prompt;
    body["stream"] = false;

    auto res = cli.Post("/api/generate", body.dump(), "application/json");

    if (!res || res->status != 200)
        return "OLLAMA ERROR";

    auto json = nlohmann::json::parse(res->body);
    return json.value("response", "");
}


double GetSimilarityScore(
    std::string_view model,
    std::string_view query,
    std::string_view description,
    int port
    ) {
    std::string prompt =
        "You are a similarity scoring system.\n\n"
        "User query:\n\"" + std::string(query) + "\"\n\n"
                               "Image description:\n\"" + std::string(description) + "\"\n\n"
                                     "Return ONLY a number from 0 to 100 representing similarity.\n"
                                     "No explanation. Only the number.";

    std::string response = AskLLaVAText(model, prompt, port);

    try {
        return std::stod(response);
    } catch (...) {
        return 0.0;
    }
}

std::vector<std::string> RankImagesByQuery(
    std::vector<Image>& images,
    std::string_view model,
    std::string_view query,
    int port
    ) {
    std::vector<std::pair<std::string, double>> scored;

    for (auto& img : images) {

        if (img.GetDescription().empty())
            continue;

        double score = GetSimilarityScore(
            model,
            query,
            img.GetDescription(),
            port
            );

        scored.emplace_back(img.GetPath(), score);
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {return a.second > b.second;});

    std::vector<std::string> result;
    for (const auto& [path, score] : scored) {
        result.push_back(path);
    }

    return result;
}


void LoadDescriptionsFromFile(std::vector<Image>& images, const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return;

    nlohmann::json j;
    try {
        file >> j;
    } catch (...) {
        std::cerr << "JSON read error\n";
        return;
    }

    for (auto& img : images) {
        std::string path = img.GetPath();

        if (j.contains(path)) {
            img.SetDescription(j[path].get<std::string>());
        }
    }
}


void SaveDescriptionsToFile(const std::vector<Image>& images, const std::string& filename) {
    nlohmann::json j;

    std::ifstream in_file(filename);
    if (in_file) {
        try {
            in_file >> j;
        } catch (...) {
            j = nlohmann::json::object();
        }
    }

    for (const auto& img : images) {
        if (!img.GetDescription().empty()) {
            j[img.GetPath()] = img.GetDescription();
        }
    }

    std::ofstream out_file(filename);
    if (!out_file) {
        std::cerr << "Cannot write file: " << filename << std::endl;
        return;
    }

    out_file << j.dump(4);
}

