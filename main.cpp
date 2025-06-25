#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <filesystem>
#include <regex>
#include <iomanip>
#include <sstream>
#include <curl/curl.h>

namespace fs = std::filesystem;

// ANSI Color codes for clean output
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
}

// Clean design elements
namespace Design {
    void printHeader() {
        std::cout << Colors::CYAN << Colors::BOLD;
        std::cout << "██╗███╗   ███╗ ██████╗ ██╗     ██╗███████╗██╗  ██╗\n";
        std::cout << "██║████╗ ████║██╔════╝ ██║     ██║██╔════╝╚██╗██╔╝\n";
        std::cout << "██║██╔████╔██║██║  ███╗██║     ██║█████╗   ╚███╔╝ \n";
        std::cout << "██║██║╚██╔╝██║██║   ██║██║     ██║██╔══╝   ██╔██╗ \n";
        std::cout << "██║██║ ╚═╝ ██║╚██████╔╝███████╗██║███████╗██╔╝ ██╗\n";
        std::cout << "╚═╝╚═╝     ╚═╝ ╚═════╝ ╚══════╝╚═╝╚══════╝╚═╝  ╚═╝\n";
        std::cout << Colors::RESET << "\n";
        std::cout << Colors::YELLOW << "High-Performance Manga Image Link Extractor\n";
        std::cout << Colors::DIM << "Version 1.0 - Built with C++17\n" << Colors::RESET;
        std::cout << "\n";
    }
    
    void printUsage(const std::string& programName) {
        std::cout << Colors::YELLOW << Colors::BOLD << "USAGE:" << Colors::RESET << "\n";
        std::cout << "   " << Colors::GREEN << programName << Colors::WHITE << " <filename> <start_chapter> <end_chapter>" << Colors::RESET << "\n\n";
        std::cout << Colors::YELLOW << Colors::BOLD << "EXAMPLES:" << Colors::RESET << "\n";
        std::cout << "   " << Colors::DIM << "# Process chapters 1-100 from manga.txt" << Colors::RESET << "\n";
        std::cout << "   " << Colors::GREEN << programName << Colors::WHITE << " manga.txt 1 100" << Colors::RESET << "\n\n";
        std::cout << "   " << Colors::DIM << "# Process chapters 50-75" << Colors::RESET << "\n";
        std::cout << "   " << Colors::GREEN << programName << Colors::WHITE << " chapters.txt 50 75" << Colors::RESET << "\n\n";
    }
    
    void printSeparator(char symbol = '-', int length = 60) {
        std::cout << Colors::CYAN;
        for (int i = 0; i < length; ++i) {
            std::cout << symbol;
        }
        std::cout << Colors::RESET << "\n";
    }
    
    void printProgress(const std::string& message, const std::string& color = Colors::BLUE) {
        std::cout << color << "> " << Colors::BOLD << message << Colors::RESET << "\n";
    }
    
    void printSuccess(const std::string& message) {
        std::cout << Colors::GREEN << "[SUCCESS] " << Colors::BOLD << message << Colors::RESET << "\n";
    }
    
    void printWarning(const std::string& message) {
        std::cout << Colors::YELLOW << "[WARNING] " << Colors::BOLD << message << Colors::RESET << "\n";
    }
    
    void printError(const std::string& message) {
        std::cout << Colors::RED << "[ERROR] " << Colors::BOLD << message << Colors::RESET << "\n";
    }
    
    void printStats(const std::string& label, const std::string& value, const std::string& unit = "") {
        std::cout << Colors::CYAN << "   * " << Colors::WHITE << std::left << std::setw(20) << label 
                  << Colors::YELLOW << Colors::BOLD << std::right << std::setw(10) << value 
                  << Colors::DIM << " " << unit << Colors::RESET << "\n";
    }
}

struct WriteCallbackData {
    std::string data;
};

// Callback function for libcurl to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, WriteCallbackData* userp) {
    size_t totalSize = size * nmemb;
    userp->data.append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

class ImgLiex {
private:
    std::unordered_map<int, std::string> chapterLinks;
    std::unordered_map<int, size_t> expectedCountsCache;
    std::string folderName;
    int processedCount = 0;
    int skippedCount = 0;
    int errorCount = 0;
    std::mutex statsMutex;
    
public:
    ImgLiex(const std::string& filename) : folderName(fs::path(filename).stem()) {
        fs::create_directories(folderName);
    }
    
    bool extractChapterLinks(const std::string& inputFile) {
        std::ifstream file(inputFile);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << inputFile << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("# Chapter") == 0) {
                // Extract chapter number
                std::regex chapterRegex(R"(# Chapter (\d+))");
                std::smatch match;
                if (std::regex_search(line, match, chapterRegex)) {
                    int chapterNum = std::stoi(match[1].str());
                    
                    // Get next line as URL
                    if (std::getline(file, line)) {
                        // Trim whitespace
                        line.erase(0, line.find_first_not_of(" \t\r\n"));
                        line.erase(line.find_last_not_of(" \t\r\n") + 1);
                        chapterLinks[chapterNum] = line;
                    }
                }
            }
        }
        
        Design::printProgress("Loaded chapter links", Colors::GREEN);
        Design::printStats("Total chapters", std::to_string(chapterLinks.size()));
        Design::printSeparator();
        return true;
    }
    
    std::vector<std::string> extractImageLinks(const std::string& htmlContent) {
        std::vector<std::string> imageLinks;
        
        // Simple regex to find img tags with class="imgholder"
        std::regex imgRegex(R"(<img[^>]*class=["\']imgholder["\'][^>]*src=["\']([^"\']+)["\'][^>]*>)");
        std::regex imgRegex2(R"(<img[^>]*src=["\']([^"\']+)["\'][^>]*class=["\']imgholder["\'][^>]*>)");
        
        std::sregex_iterator start(htmlContent.begin(), htmlContent.end(), imgRegex);
        std::sregex_iterator end;
        
        for (std::sregex_iterator i = start; i != end; ++i) {
            std::smatch match = *i;
            imageLinks.push_back(match[1].str());
        }
        
        // Try second pattern if first didn't match
        if (imageLinks.empty()) {
            start = std::sregex_iterator(htmlContent.begin(), htmlContent.end(), imgRegex2);
            for (std::sregex_iterator i = start; i != end; ++i) {
                std::smatch match = *i;
                imageLinks.push_back(match[1].str());
            }
        }
        
        return imageLinks;
    }
    
    std::string downloadHTML(const std::string& url) {
        CURL* curl;
        CURLcode res;
        WriteCallbackData data;
        
        curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "imgliex/1.0");
        
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("CURL error: " + std::string(curl_easy_strerror(res)));
        }
        
        return data.data;
    }
    
    void saveImageLinks(int chapterNum, const std::vector<std::string>& imageLinks) {
        fs::path chapterFolder = fs::path(folderName) / ("chapter-" + std::to_string(chapterNum));
        fs::create_directories(chapterFolder);
        
        std::ofstream baseFile(chapterFolder / "base.txt");
        if (!baseFile.is_open()) {
            throw std::runtime_error("Cannot create base.txt for chapter " + std::to_string(chapterNum));
        }
        
        for (const auto& link : imageLinks) {
            baseFile << link << "\n";
        }
    }
    
    size_t countLinesfast(const fs::path& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return 0;
        
        size_t count = 0;
        std::string line;
        while (std::getline(file, line)) {
            ++count;
        }
        return count;
    }
    
    bool chapterAlreadyProcessed(int chapterNum, size_t expectedImgCount) {
        fs::path chapterFolder = fs::path(folderName) / ("chapter-" + std::to_string(chapterNum));
        fs::path baseFile = chapterFolder / "base.txt";
        
        if (!fs::exists(baseFile)) {
            return false;
        }
        
        size_t lineCount = countLinesfast(baseFile);
        return lineCount == expectedImgCount;
    }
    
    void processChapter(int chapterNum, const std::string& url) {
        try {
            size_t expectedImgCount;
            
            // Check cache first
            auto cacheIt = expectedCountsCache.find(chapterNum);
            if (cacheIt != expectedCountsCache.end()) {
                expectedImgCount = cacheIt->second;
            } else {
                std::string htmlContent = downloadHTML(url);
                std::vector<std::string> imageLinks = extractImageLinks(htmlContent);
                expectedImgCount = imageLinks.size();
                expectedCountsCache[chapterNum] = expectedImgCount;
                
                // Save immediately since we already have the data
                if (!chapterAlreadyProcessed(chapterNum, expectedImgCount)) {
                    saveImageLinks(chapterNum, imageLinks);
                    {
                        std::lock_guard<std::mutex> lock(statsMutex);
                        processedCount++;
                    }
                    std::cout << Colors::GREEN << "[OK] " << Colors::WHITE << "Chapter " << Colors::YELLOW << std::setw(3) << chapterNum 
                              << Colors::WHITE << " -> " << Colors::CYAN << expectedImgCount << Colors::DIM << " images" << Colors::RESET << "\n";
                    return;
                }
            }
            
            // Check if already processed
            if (chapterAlreadyProcessed(chapterNum, expectedImgCount)) {
                {
                    std::lock_guard<std::mutex> lock(statsMutex);
                    skippedCount++;
                }
                std::cout << Colors::YELLOW << "[SKIP] " << Colors::WHITE << "Chapter " << Colors::YELLOW << std::setw(3) << chapterNum 
                          << Colors::DIM << " -> already processed (" << expectedImgCount << " images)" << Colors::RESET << "\n";
                return;
            }
            
            // Process if not cached and not processed
            std::string htmlContent = downloadHTML(url);
            std::vector<std::string> imageLinks = extractImageLinks(htmlContent);
            
            saveImageLinks(chapterNum, imageLinks);
            {
                std::lock_guard<std::mutex> lock(statsMutex);
                processedCount++;
            }
            std::cout << Colors::GREEN << "[OK] " << Colors::WHITE << "Chapter " << Colors::YELLOW << std::setw(3) << chapterNum 
                      << Colors::WHITE << " -> " << Colors::CYAN << imageLinks.size() << Colors::DIM << " images" << Colors::RESET << "\n";
            
        } catch (const std::exception& e) {
            {
                std::lock_guard<std::mutex> lock(statsMutex);
                errorCount++;
            }
            std::cout << Colors::RED << "[FAIL] " << Colors::WHITE << "Chapter " << Colors::YELLOW << std::setw(3) << chapterNum 
                      << Colors::RED << " -> " << e.what() << Colors::RESET << "\n";
        }
    }
    
    void processChapters(int startChapter, int endChapter, int numThreads = 4) {
        Design::printProgress("Starting processing", Colors::MAGENTA);
        Design::printStats("Chapter range", std::to_string(startChapter) + " - " + std::to_string(endChapter));
        Design::printStats("Threads", std::to_string(numThreads));  
        Design::printStats("Output folder", folderName);
        Design::printSeparator();
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<void>> futures;
        
        for (int chapterNum = startChapter; chapterNum <= endChapter; ++chapterNum) {
            auto it = chapterLinks.find(chapterNum);
            if (it != chapterLinks.end()) {
                futures.push_back(
                    std::async(std::launch::async, [this, chapterNum, url = it->second]() {
                        this->processChapter(chapterNum, url);
                    })
                );
                
                // Limit concurrent threads
                if (futures.size() >= static_cast<size_t>(numThreads)) {
                    for (auto& future : futures) {
                        future.wait();
                    }
                    futures.clear();
                }
            } else {
                Design::printWarning("Chapter " + std::to_string(chapterNum) + " not found in input file");
            }
        }
        
        // Wait for remaining futures
        for (auto& future : futures) {
            future.wait();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Print final statistics
        std::cout << "\n";
        Design::printSeparator('=');
        Design::printSuccess("Processing Complete!");
        Design::printSeparator();
        
        Design::printStats("Processed", std::to_string(processedCount), "chapters");
        Design::printStats("Skipped", std::to_string(skippedCount), "chapters");
        Design::printStats("Errors", std::to_string(errorCount), "chapters");
        Design::printStats("Total time", std::to_string(duration.count() / 1000.0), "seconds");
        
        if (processedCount > 0) {
            double avgTime = (duration.count() / 1000.0) / processedCount;
            Design::printStats("Avg per chapter", std::to_string(avgTime), "sec/chapter");
        }
        
        Design::printSeparator();
        std::cout << Colors::CYAN << "Results saved in: " << Colors::YELLOW << Colors::BOLD 
                  << folderName << Colors::RESET << "\n\n";
        
        Design::printSeparator('=');
    }
};

int main(int argc, char* argv[]) {
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Print clean header
    Design::printHeader();
    
    // Check command line arguments
    if (argc != 4) {
        Design::printError("Invalid number of arguments!");
        Design::printUsage(argv[0]);
        Design::printSeparator('=');
        curl_global_cleanup();
        return 1;
    }
    
    try {
        std::string filename = argv[1];
        int startChapter = std::stoi(argv[2]);
        int endChapter = std::stoi(argv[3]);
        
        if (startChapter > endChapter) {
            Design::printError("Start chapter cannot be greater than end chapter!");
            curl_global_cleanup();
            return 1;
        }
        
        if (startChapter < 1 || endChapter < 1) {
            Design::printError("Chapter numbers must be positive!");
            curl_global_cleanup();
            return 1;
        }
        
        // Check if input file exists
        if (!fs::exists(filename)) {
            Design::printError("Input file '" + filename + "' not found!");
            curl_global_cleanup();
            return 1;
        }
        
        Design::printProgress("Initializing ImgLiex", Colors::BLUE);
        ImgLiex extractor(filename);
        
        Design::printProgress("Loading chapter links from " + filename, Colors::BLUE);
        if (!extractor.extractChapterLinks(filename)) {
            Design::printError("Failed to load chapter links!");
            curl_global_cleanup();
            return 1;
        }
        
        // Auto-detect optimal thread count (max 8 to be nice to servers)
        int numThreads = std::min(8, static_cast<int>(std::thread::hardware_concurrency()));
        if (numThreads == 0) numThreads = 4; // fallback
        
        extractor.processChapters(startChapter, endChapter, numThreads);
        
    } catch (const std::invalid_argument& e) {
        Design::printError("Invalid chapter number format!");
        Design::printUsage(argv[0]);
        curl_global_cleanup();
        return 1;
    } catch (const std::exception& e) {
        Design::printError("Unexpected error: " + std::string(e.what()));
        curl_global_cleanup();
        return 1;
    }
    
    // Cleanup CURL
    curl_global_cleanup();
    return 0;
}