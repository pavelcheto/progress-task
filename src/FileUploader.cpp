#include "FileUploader.h"

#include "json.hpp"

#include <curl/curl.h>
#include <iostream>
#include <string>
#include <functional>
#include <fstream>
#include <filesystem>

static const std::string url {"https://mobile-1.moveitcloud.com/"};

class OnScopeExit {
public:
    OnScopeExit(std::function<void()> func) : func {func} {}
    ~OnScopeExit() {
        func();
    }
private:
    std::function<void()> func;
};

struct FileReading {
    std::string name;
    std::ifstream stream;
    long long totalSize {0};
    long long readSize {0};
    short currentPercentage {-1};
};

class FileUploader {
public:
    ~FileUploader();

    bool initCurl();
    bool retrieveToken(std::string_view user, std::string_view password);
    bool retrieveHomeFolderId();
    bool setupFileData(std::string_view file);
    bool checkFileOnServer();
    void sendFile();

private:
    void initNewRequest(const std::string& endpoint, std::string& response);
    curl_slist* setHeader(const std::vector<std::string>& items);
    bool performRequest();

    CURL* curl {nullptr};
    std::string token;
    long long homeFolderId {0};
    FileReading fileData;
};

FileUploader::~FileUploader()
{
    curl_easy_cleanup(curl);
}

bool FileUploader::initCurl()
{
    curl = curl_easy_init();
    if (curl == nullptr) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }

    return true;
}

size_t writeCallback(void* buffer, size_t size, size_t nmemb, std::string* result)
{
    const size_t actualSize = size * nmemb;
    result->append(static_cast<char*>(buffer), actualSize);
    return actualSize;
}

void FileUploader::initNewRequest(const std::string& endpoint, std::string& response)
{
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
}

curl_slist* FileUploader::setHeader(const std::vector<std::string>& items)
{
    curl_slist* header {nullptr};
    for (const auto& item : items) {
        header = curl_slist_append(header, item.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

    return header;
}

bool FileUploader::performRequest()
{
    const auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        std::cerr << "Request failed: " << curl_easy_strerror(result) << std::endl;
        return false;
    }

    return true;
}

bool FileUploader::retrieveToken(std::string_view user, std::string_view password)
{
    OnScopeExit resetCurl {[curl=curl] () { curl_easy_reset(curl); }};
    const std::string endpoint {url + "api/v1/token"};
    std::string response;
    initNewRequest(endpoint, response);

    const std::vector<std::string> headerItems {{"Content-Type: application/x-www-form-urlencoded"}};
    auto* header = setHeader(headerItems);
    OnScopeExit freeHeader {[header] () { curl_slist_free_all(header); }};

    std::string body{"grant_type=password&username="};
    body += user;
    body += "&password=";
    body += password;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    if (!performRequest()) {
        return false;
    }

    try {
        const auto json = nlohmann::json::parse(response);
        token = json.at("access_token").template get<std::string>();
        return true;
    }
    catch (...) {
        std::cerr << "Error extracting token" << std::endl;
    }

    return false;
}

bool FileUploader::retrieveHomeFolderId()
{
    OnScopeExit resetCurl {[curl=curl] () { curl_easy_reset(curl); }};
    const std::string endpoint {url +"api/v1/users/self"};
    std::string response;
    initNewRequest(endpoint, response);

    std::string authorization {"Authorization: Bearer "};
    authorization += token;
    const std::vector<std::string> headerItems {{"Content-Type: application/json"}, {std::move(authorization)}};
    auto* header = setHeader(headerItems);
    OnScopeExit freeHeader {[header] () { curl_slist_free_all(header); }};

    if (!performRequest()) {
        return false;
    }

    try {
        const auto json = nlohmann::json::parse(response);
        homeFolderId = json.at("homeFolderID").template get<long long>();
        return true;
    }
    catch (...) {
        std::cerr << "Error extracting home folder id" << std::endl;
    }

    return false;
}

bool FileUploader::setupFileData(std::string_view file)
{
    fileData.stream.open(file.data(), std::ios::in | std::ios::binary);
    if (!fileData.stream.is_open()) {
        std::cerr << "Failed to open file" << std::endl;
        return false;
    }

    fileData.stream.seekg(0, std::ios::end);
    fileData.totalSize = fileData.stream.tellg();
    fileData.stream.seekg(0);

    fileData.name = std::filesystem::weakly_canonical(file).filename();

    return true;
}

bool FileUploader::checkFileOnServer()
{
    OnScopeExit resetCurl {[curl=curl] () { curl_easy_reset(curl); }};
    const std::string endpoint {url + "api/v1/folders/" + std::to_string(homeFolderId) + "/files"};
    std::string response;
    initNewRequest(endpoint, response);

    std::string authorization {"Authorization: Bearer "};
    authorization += token;
    const std::vector<std::string> headerItems {{"Content-Type: multipart/form-data"}, {std::move(authorization)}};
    auto* header = setHeader(headerItems);
    OnScopeExit freeHeader {[header] () { curl_slist_free_all(header); }};

    if (!performRequest()) {
        return false;
    }

    try {
        const auto json = nlohmann::json::parse(response);
        const auto items = json.at("items");
        if (!items.is_array()) {
            return false;
        }

        for (const auto& item : items) {
            const auto serverFileName = item.at("name").template get<std::string>();
            if (serverFileName == fileData.name) {
                std::cerr << "File already exists on server. Upload failed." << std::endl;
                return false;
            }
        }

        return true;
    }
    catch (...) {
        std::cerr << "Error in file lookup on server" << std::endl;
    }

    return false;
}

size_t fileRead(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* fileData = static_cast<FileReading*>(userdata);
    const auto totalSize = size * nitems;
    if (!fileData->stream.read(buffer, totalSize)) {
        const auto read = fileData->stream.gcount();
        if (read == 0) {
            std::cout << "\rFinished 100%" << std::endl;
        }
        return read;
    }
    fileData->readSize += totalSize;
    const auto newPercentage = (fileData->readSize * 100) / fileData->totalSize;
    if (newPercentage != fileData->currentPercentage) {
        fileData->currentPercentage = newPercentage;
        std::cout << "\rProgress: " << newPercentage << "%" << std::flush;
    }

    return totalSize;
}

void FileUploader::sendFile()
{
    OnScopeExit resetCurl {[curl=curl] () { curl_easy_reset(curl); }};
    const std::string endpoint {url + "api/v1/folders/" + std::to_string(homeFolderId) + "/files"};
    std::string response;
    initNewRequest(endpoint, response);

    std::string authorization {"Authorization: Bearer "};
    authorization += token;
    const std::vector<std::string> headerItems {{"Content-Type: multipart/form-data"}, {std::move(authorization)}};
    auto* header = setHeader(headerItems);
    OnScopeExit freeHeader {[header] () { curl_slist_free_all(header); }};

    curl_mime* mime = curl_mime_init(curl);
    OnScopeExit mimeFree {[mime] () { curl_mime_free(mime); }};

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filename(part, fileData.name.c_str());

    curl_mime_data_cb(part, -1, fileRead, nullptr, nullptr, &fileData);

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    std::cout << "Starting file upload" << std::endl;
    if (!performRequest()) {
        return;
    }

    long httpCode {0};
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (httpCode / 100 != 2) {
        std::cerr << "Uploading file failed:" << std::endl << response << std::endl;
    }
}

void uploadFile(std::string_view user, std::string_view password, std::string_view file)
{
    FileUploader uploader;

    if (!uploader.initCurl()) {
        return;
    }
    if (!uploader.retrieveToken(user, password)) {
        return;
    }
    if (!uploader.retrieveHomeFolderId()) {
        return;
    }
    if (!uploader.setupFileData(file)) {
        return;
    }
    if (!uploader.checkFileOnServer()) {
        return;
    }
    uploader.sendFile();
}