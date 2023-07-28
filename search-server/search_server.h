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
#include <execution>
#include <list>
#include <future>
#include <mutex>
#include <unordered_set>

#include "document.h"
#include "string_processing.h"


#include "concurrent_map.h"

using namespace std::literals::string_literals; // шта? а как приставить к оператору s std?


// нешаблонные выносить
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 0.0001;

class SearchServer
{
public:

    explicit SearchServer(const std::string &stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    std::set<int>::const_iterator begin();
    std::set<int>::const_iterator end();
    void AddDocument(int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings);
    int GetDocumentCount() const;
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);
    void RemoveDocument(int document_id);
    static std::vector<std::string_view>  SplitIntoWords(std::string_view text) ;
    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
    {
        auto container = MakeUniqueNonEmptyStrings(stop_words);
        for (const auto &word : container)
        {
            if (!IsValidWord(std::string_view(word)))
            {
                throw std::invalid_argument("Invalid stop word");
            }
        }
        stop_words_ = container;
    }
    std::vector<Document> FindTopDocuments( const std::string_view raw_query, DocumentStatus status_seek ) const;
    std::vector<Document> FindTopDocuments( const std::string_view raw_query) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments( ExecutionPolicy policy , const std::string_view raw_query, DocumentStatus status_seek  ) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments( ExecutionPolicy policy, const std::string_view raw_query  ) const;

    template <typename Predicate>
    std::vector<Document> FindTopDocuments( const std::string_view raw_query,
                                            Predicate predicate ) const;

    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindTopDocuments( ExecutionPolicy policy , const std::string_view raw_query,
                                            Predicate predicate ) const;
private:

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document2words_freqs;
    std::list<std::string> documents_texts;
    std::map<int, DocumentData> documents_;
    std::set<int> index_to_id;


    bool IsValidWord(const std::string_view word) const;
    bool IsStopWord(const std::string_view word) const;




    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;


    static int ComputeAverageRating(const std::vector<int> &ratings);
    QueryWord ParseQueryWord(std::string_view text) const;
    bool IsInvalidQueryWord(std::string_view word) const;
    Query ParseQuery(const std::string_view text) const;
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments( const Query &query, Predicate predicate) const;

    template <typename Predicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(ExecutionPolicy policy, const Query &query, Predicate predicate) const ;
};




template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments( ExecutionPolicy policy , const std::string_view raw_query, DocumentStatus status_seek  ) const{
    return SearchServer::FindTopDocuments(policy ,raw_query, [status_seek]([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating)
    { return status == status_seek; } );
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments( ExecutionPolicy policy, const std::string_view raw_query  ) const{
    return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL );
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments( const std::string_view raw_query,
                                        Predicate predicate ) const{
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, predicate );

}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments( ExecutionPolicy policy , const std::string_view raw_query,
                                        Predicate predicate ) const
{

    Query query = ParseQuery(raw_query);

    std::vector<Document> result = SearchServer::FindAllDocuments(policy, query, predicate);

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

template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments( const Query &query, Predicate predicate) const{
    return SearchServer::FindAllDocuments(std::execution::seq, query, predicate);
}

template <typename Predicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy policy, const Query &query, Predicate predicate) const {



    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::parallel_policy>){
        ConcurrentMap<int, double> document_to_relevance(1000);
        std::for_each(policy, query.plus_words.begin(), query.plus_words.end(),
                      [&](auto& word) {
                          if (word_to_document_freqs_.count(word) == 0) {
                              return;
                          }
                          const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
                          for (const auto [document_id, term_freq]: word_to_document_freqs_.at(word)) {
                              if (predicate(document_id, documents_.at(document_id).status,
                                            documents_.at(document_id).rating)) {
                                  document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                              }
                          }
                      });
        std::for_each(policy,query.minus_words.begin(), query.minus_words.end(),[&](auto& word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        });
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance]: document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }else {
        std::map<int, double> document_to_relevance;
        for (const std::string_view word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (std::string_view word : query.minus_words)
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


}