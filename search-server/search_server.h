#pragma once

#include <vector>
#include <map>
#include <string>
#include <utility>
#include <set>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <functional>

#include "document.h"
#include "string_processing.h"

// using namespace std::literals::string_literals; // шта? а как приставить к оператору s std?


// нешаблонные выносить 
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 0.0001;

class SearchServer
{
public:
   
    explicit SearchServer(const std::string &stop_words_text);
    int GetDocumentId(int index);
    void AddDocument(int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings);
    int GetDocumentCount() const;

    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentStatus status_seek) const;
    std::vector<Document> FindTopDocuments(const std::string &raw_query) const;
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string &raw_query,int document_id) const;


    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
    {
        auto container = MakeUniqueNonEmptyStrings(stop_words);
        for (const auto &word : container)
        {
            if (!IsValidWord(word))
            {
                throw std::invalid_argument("Invalid stop word");
            }
        }
        stop_words_ = container;
    }

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string &raw_query,
                                           Predicate predicate) const
    {

        Query query = ParseQuery(raw_query);

        std::vector<Document> result = FindAllDocuments(query, predicate);

        sort(result.begin(), result.end(),
             [](const Document &lhs, const Document &rhs)
             {
                 if (std::abs(lhs.relevance - rhs.relevance) < EPS)
                 {
                     return lhs.rating > rhs.rating;
                 }
                 else
                 {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return result;
    }

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };
    struct QueryWord
    {
        std::string data;
        bool is_minus;
        bool is_stop;
    };
    struct Query
    {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> index_to_id;

    bool IsValidWord(const std::string &word) const;
    bool IsStopWord(const std::string &word) const;
    std::vector<std::string> SplitIntoWordsNoStop(const std::string &text) const;
    std::vector<std::string> SplitIntoWords(const std::string &text) const;
    static int ComputeAverageRating(const std::vector<int> &ratings);
    QueryWord ParseQueryWord(std::string text) const;
    bool IsInvalidQueryWord(std::string word) const;
    Query ParseQuery(const std::string &text) const;
    double ComputeWordInverseDocumentFreq(const std::string &word) const;


    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const Query &query, Predicate predicate) const
    {
        std::map<int, double> document_to_relevance;
        for (const std::string &word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const std::string &word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};
