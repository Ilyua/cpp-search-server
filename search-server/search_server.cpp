#include "search_server.h"


using namespace std;


SearchServer::SearchServer(const std::string &stop_words_text)
        : SearchServer(
        SplitIntoWords(std::string_view(stop_words_text))) // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(std::string_view stop_words_text)
        : SearchServer(
        SplitIntoWords(stop_words_text)) // Invoke delegating constructor from string container
{
}



set<int>::const_iterator SearchServer::begin()

{
    const auto it = index_to_id.begin();
    return it;
}
set<int>::const_iterator SearchServer::end()

{
    const auto it = index_to_id.end();
    return it;
}

const map<string_view, double>&  SearchServer::GetWordFrequencies(const int document_id) const{
    static const map<string, double> res;

    auto resultIt = document2words_freqs.find(document_id);

    if (resultIt != document2words_freqs.end())
    {
        return resultIt -> second;
    }
    else
    {
        static const map<string_view, double> res;
        return res;
    }
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

    documents_texts.push_back(document);

    const std::vector<std::string_view> words = SplitIntoWordsNoStop(documents_texts.back());
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document2words_freqs[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    index_to_id.insert(document_id);
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
void SearchServer::RemoveDocument(execution::parallel_policy policy, int document_id)
{
    if( document2words_freqs.count(document_id) == 0)
        return;

    std::map<std::string_view, double> words_by_id = GetWordFrequencies(document_id);
     vector< std::string_view >  wordsDel(words_by_id.size());

    std::transform(std::execution::par,words_by_id.begin(), words_by_id.end(),
                   wordsDel.begin(), // write to the same location
                   [](  auto   x) { return   x.first; });

    std::for_each(std::execution::par,wordsDel.begin(),wordsDel.end() ,[&document_id, this](string_view word){ word_to_document_freqs_.at(word).erase(document_id);} );

    document2words_freqs.erase(document_id);
    documents_.erase(document_id);
    index_to_id.erase( document_id);

}



void SearchServer::RemoveDocument(execution::sequenced_policy policy, int document_id)
{
    if( document2words_freqs.count(document_id) == 0)
        return;

    for (auto& [word, freq] : document2words_freqs.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    document2words_freqs.erase(document_id);
    documents_.erase(document_id);
    index_to_id.erase( document_id);

}

void SearchServer::RemoveDocument(int document_id){
    RemoveDocument(std::execution::seq,  document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(std::execution::sequenced_policy policy, const std::string &raw_query, int document_id) const {

    const Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;

    for (const std::string_view word: query.minus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it == word_to_document_freqs_.end()) {
            continue;
        }
        if (it->second.count(document_id)) {
            return {matched_words, documents_.at(document_id).status};
        }
    }

    for (std::string_view word: query.plus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it == word_to_document_freqs_.end()) {
            continue;
        }
        if (it->second.count(document_id)) {
            matched_words.push_back(word);
        }
    }



    return {matched_words, documents_.at(document_id).status};
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy, const std::string &raw_query,int document_id) const{

//     Query query = ParseQuery(raw_query);
    if (!documents_.count(document_id)) {
        throw out_of_range("Недействительный id документа"s);
    }
    vector<string_view> minusWords ;
    vector<string_view> plusWords ;

    for (std::string_view word : SplitIntoWords(raw_query))
    {

        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                minusWords.push_back(query_word.data);
            }
            else
            {
                plusWords.push_back(query_word.data);
            }
        }
    }

    std::vector<std::string_view> matched_words;


    if (std::any_of(policy , minusWords.begin(), minusWords.end(),[document_id, this](string_view word){ return word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id) > 0; })){
        return {matched_words, documents_.at(document_id).status};

    }
    matched_words.resize(plusWords.size());


    auto last_copied = std::copy_if(policy, plusWords.begin(), plusWords.end(),matched_words.begin(), [document_id, this](string_view word)
    {return word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id) > 0 ;});

    matched_words.erase(last_copied, matched_words.end());
    std::sort(matched_words.begin(), matched_words.end());
    auto last = std::unique(policy , matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

    return {matched_words, documents_.at(document_id).status};


}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string &raw_query,
                                                                                    int document_id) const
{
   return MatchDocument(std::execution::seq, raw_query,document_id);
}
int SearchServer::GetDocumentCount() const
{
    return static_cast<int>(documents_.size());
}

bool SearchServer::IsValidWord(const std::string_view word) const
{
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c)
                        { return c >= '\0' && c < ' '; });
}

bool SearchServer::IsStopWord(const std::string_view word) const
{
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const
{
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text))
    {
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}



std::vector<std::string_view> SearchServer::SplitIntoWords(std::string_view text)
{
    std::vector<std::string_view> words;
    std::string_view word;

    // left trim
    size_t pos = text.find_first_not_of(' ');
    text.remove_prefix(std::min(text.size(), pos));

    // rigt trim
    pos = text.find_last_not_of(' ');
    text.remove_suffix(std::min(text.size() , text.size() - pos - 1));

    while (!text.empty()) {
        auto pos =  text.find(' ');

        word = text.substr(0, std::min(text.size() , pos));

        text.remove_prefix(std::min(text.size(), pos));
        pos = text.find_first_not_of(' ');

        text.remove_prefix(std::min(text.size(), pos));

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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
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

    bool SearchServer::IsInvalidQueryWord(std::string_view word) const
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




SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const
{
    Query query;

    for (std::string_view word : SplitIntoWords(text))
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
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const
{
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}


