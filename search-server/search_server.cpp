#include "search_server.h"


using namespace std;

SearchServer::SearchServer(const std::string &stop_words_text)
    : SearchServer(
          SplitIntoWords(stop_words_text)) // Invoke delegating constructor from string container
{
}

int SearchServer::GetDocumentId(int index)
{
    return index_to_id.at(index);
}

void SearchServer::AddDocument(int document_id, const std::string &document,
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

    std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query, DocumentStatus status_seek) const
{

    return FindTopDocuments(raw_query, [status_seek]([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating)
                            { return status == status_seek; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string &raw_query,
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
int SearchServer::GetDocumentCount() const
{
    return static_cast<int>(documents_.size());
}

bool SearchServer::IsValidWord(const std::string &word) const
{
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c)
                        { return c >= '\0' && c < ' '; });
}

bool SearchServer::IsStopWord(const std::string &word) const
{
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string &text) const
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
std::vector<std::string> SearchServer::SplitIntoWords(const std::string &text) const
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

int SearchServer :: ComputeAverageRating(const std::vector<int> &ratings)
{
    if (ratings.empty())
    {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const
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

    bool SearchServer::IsInvalidQueryWord(std::string word) const
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

SearchServer::Query SearchServer::ParseQuery(const std::string &text) const
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
double SearchServer::ComputeWordInverseDocumentFreq(const std::string &word) const
{
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}


