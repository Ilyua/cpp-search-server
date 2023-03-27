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

#include "string_processing.h"
#include "document.h"

// using namespace std::literals::string_literals; // шта? а как приставить к оператору s std?


// нешаблонные выносить 
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 0.0001;

class SearchServer
{
public:
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

    explicit SearchServer(const std::string &stop_words_text)
        : SearchServer(
              SplitIntoWords(stop_words_text)) // Invoke delegating constructor from string container
    {
    }

    int GetDocumentId(int index)
    {
        return index_to_id.at(index);
    }

    void AddDocument(int document_id, const std::string &document,
                     DocumentStatus status, const std::vector<int> &ratings)
    {
        if (document_id < 0)
        {
            throw std::invalid_argument("document_id < 0");
        }
        if (documents_.count(document_id) > 0)
        {
            throw std::invalid_argument("document_id already exists");
        }
        if (!IsValidWord(document))
        {
            throw std::invalid_argument("document containse resticted symbols");
        }

        const std::vector<std::string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const std::string &word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        index_to_id.push_back(document_id);
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
                 if (std::abs(lhs.relevance - rhs.relevance) < 1e-6)
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

    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentStatus status_seek) const
    {

        return FindTopDocuments(raw_query, [status_seek]([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]]  int rating)
                                { return status == status_seek; });
    }

    std::vector<Document> FindTopDocuments(const std::string &raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string &raw_query,
                                                             int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        std::vector<std::string> matched_words;
        for (const std::string &word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }
        for (const std::string &word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
    int GetDocumentCount() const
    {
        return static_cast<int>(documents_.size());
    }

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> index_to_id;

    bool IsValidWord(const std::string &word) const
    {
        // A valid word must not contain special characters
        return std::none_of(word.begin(), word.end(), [](char c)
                       { return c >= '\0' && c < ' '; });
    }

    bool IsStopWord(const std::string &word) const
    {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string &text) const
    {
        std::vector<std::string> words;
        for (const std::string &word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }
    std::vector<std::string> SplitIntoWords(const std::string &text) const
    {
        std::vector<std::string> words;
        std::string word;
        for (const char c : text)
        {
            if (c == ' ')
            {
                if (!word.empty())
                {
                    words.push_back(word);
                    word.clear();
                }
            }
            else
            {
                word += c;
            }
        }
        if (!word.empty())
        {
            words.push_back(word);
        }

        return words;
    }

    static int ComputeAverageRating(const std::vector<int> &ratings)
    {
        if (ratings.empty())
        {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings)
        {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
    struct QueryWord
    {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const
    {
        bool is_minus = false;
        if (text.size() < 1)
        {
            throw std::invalid_argument("Empty search word");
        }
        else
        {
            if ((text.size() == 1) && (text[0] == '-'))
            {
                throw std::invalid_argument("Search word consists of one minus");
            }
            if ((text.size() > 1) && (text[0] == '-') && (text[1] == '-'))
            {
                throw std::invalid_argument("Two minuses before word");
            }
            if (!IsValidWord(text))
                throw std::invalid_argument("Word contains restricted symbols");
        }

        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query
    {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };
    bool IsInvalidQueryWord(std::string word) const
    {
        if (word.size() < 1)
        {
            return true;
        }
        else
        {
            if ((word[0] == '-') && (word.size() == 1))
            {
                return true;
            }
            if ((word[0] == '-') && (word.size() > 1) && (word[1] == '-'))
            {
                return true;
            }
            if (!IsValidWord(word))
                return true;
        }
        return false;
    }

    Query ParseQuery(const std::string &text) const
    {
        Query query;
        for (const std::string &word : SplitIntoWords(text))
        {

            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
    double ComputeWordInverseDocumentFreq(const std::string &word) const
    {
        return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
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
