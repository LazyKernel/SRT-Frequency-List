#define WINVER 0x0A00
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <filesystem>
#include <map>

#include "srtparser.h"

#include "core/env.h"
#include "core/analysis/analysis_result.h"

void DieOnError(jumanpp::Status status)
{
    if (!status)
    {
        std::cerr << status << std::endl;
        exit(1);
    }
}


bool IsInStopWords(const std::string& str) 
{
    const std::vector<std::string> stopwords = { "(", ")", "　", "（", "）", "？", "！", "…", "!", "･", "”", "“", "?", "～", "―", "、" };
    for (const std::string& stopword : stopwords)
    {
        if (str == stopword)
        {
            return true;
        }
    }

    return false;
}

std::vector<std::string> LoadSrtFiles(const std::string& directory)
{
    std::vector<std::string> strings;

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (dirEntry.is_regular_file())
        {
            // This srt parser doesnt support unicode stuff apparently
            SubtitleParserFactory subParserFactory = SubtitleParserFactory(dirEntry.path().generic_string());
            SubtitleParser* parser = subParserFactory.getParser();

            for (SubtitleItem* sub : parser->getSubtitles())
            {
                strings.push_back(sub->getText());
            }

            delete parser;
        }
    }

    return strings;
}

std::map<std::string, uint32_t> ParseWordFrequencies(const std::vector<std::string>& subtitles)
{
    std::map<std::string, uint32_t> frequencies;

    jumanpp::core::JumanppEnv env;
    DieOnError(env.loadModel("model/jumandic.jppmdl"));
    
    env.setBeamSize(5);
    env.setGlobalBeam(6, 1, 5);

    DieOnError(env.initFeatures(nullptr));

    jumanpp::core::analysis::Analyzer analyzer;
    DieOnError(env.makeAnalyzer(&analyzer));

    jumanpp::core::analysis::StringField surface;
    jumanpp::core::analysis::StringField baseform;

    auto& output = analyzer.output();
    output.stringField("surface", &surface);
    output.stringField("baseform", &baseform);

    jumanpp::core::analysis::AnalysisResult resultFiller;
    jumanpp::core::analysis::AnalysisPath top1;

    jumanpp::core::analysis::NodeWalker walker;
    jumanpp::core::analysis::ConnectionPtr cptr{};

    uint64_t wordsParsed = 0;
    for (const std::string& subtitle : subtitles)
    {
        resultFiller.reset(analyzer);
        jumanpp::Status status = analyzer.analyze(subtitle);
        resultFiller.fillTop1(&top1);

        while (top1.nextBoundary())
        {
            if (!top1.nextNode(&cptr) || !output.locate(cptr.latticeNodePtr(), &walker) || !walker.next())
                break;

            std::string word = baseform[walker].str();
            if (!IsInStopWords(word))
            {
                if (frequencies.find(word) == frequencies.end())
                {
                    frequencies.insert(std::pair(word, 1));
                }
                else
                {
                    frequencies[word]++;
                }
            }

            std::cout << '\r' << "Words parsed: " << ++wordsParsed << std::flush;
        }
    }

    return frequencies;
}

// use something like that https://github.com/ku-nlp/jumanpp
// 人六大犬太丈伏牛午矢失夫天史束東朱来未末米木本体休オ才
int main()
{
    std::setlocale(LC_ALL, "en_US.UTF-8");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::vector<std::string> strings = LoadSrtFiles("srt");
    std::map<std::string, uint32_t> frequencies = ParseWordFrequencies(strings);

    std::cout << '\n' << "Writing results to output.txt" << std::endl;

    // Pushing to a vector so we can sort them
    std::vector<std::pair<std::string, uint32_t>> freqArray;
    for (auto iter = frequencies.cbegin(); iter != frequencies.cend(); ++iter)
    {
        freqArray.push_back(*iter);
    }
    std::sort(freqArray.begin(), freqArray.end(), [=](std::pair<std::string, uint32_t>& a, std::pair<std::string, uint32_t>& b) { return a.second > b.second; });

    std::ofstream outputFile;
    outputFile.open("output.txt", std::ios::trunc);
    for (const auto& freq : freqArray)
    {
        outputFile << freq.first << ' ' << freq.second << '\n';
    }
    outputFile.close();

    return 0;
}
