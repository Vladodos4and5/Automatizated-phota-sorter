#pragma once
#include "httplib.h"
#include "json.hpp"
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <string_view>

namespace fs = std::filesystem;

//----------------- Изображение -------------------------
class Image final {
public:
    Image(std::string path);

    void SetDescription(std::string_view description);
    std::string GetDescription() const;
    void CreateBase64();
    std::string GetBase64() const;
    std::string GetPath() const;
private:
    std::string path_;
    std::string description_ = "";
    std::string base64_form_ = "";
};
//--------------- Изображение end -----------------------

std::vector<Image> LoadImagesFromDirectory(const std::string& directory_path);

std::string AskGemma3_12b(std::string request); // Текстовый запрос в Гемму

std::string AskLLaVAVision(
    std::string_view model,
    std::string_view prompt,
    std::string_view image_base64,
    int port
    );

std::string Base64Encode(const unsigned char* data, size_t len);

std::string ImageToBase64(const std::string& path);

void CreateBase64ForVector(std::vector<Image>& images);

void DescribeImagesFromVector(
    std::vector<Image>& images,
    std::string_view model,
    std::string_view prompt,
    int port
    );

std::string AskLLaVAText(
    std::string_view model,
    std::string_view prompt,
    int port
    );

double GetSimilarityScore(
    std::string_view model,
    std::string_view query,
    std::string_view description,
    int port
    );

std::vector<std::string> RankImagesByQuery(
    std::vector<Image>& images,
    std::string_view model,
    std::string_view query,
    int port
    );


void SaveDescriptionsToFile(
    const std::vector<Image>& images,
    const std::string& filename
    );

void LoadDescriptionsFromFile(
    std::vector<Image>& images,
    const std::string& filename
    );
