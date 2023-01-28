#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 0.0001;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
ostream& operator<<(ostream& out, const DocumentStatus& status) {
   
        out << static_cast<int>(status);
    
    return out;
}
class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    
    template <typename Func>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Func predicat) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicat);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    
    
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_seek =  DocumentStatus::ACTUAL) const{
        return FindTopDocuments(raw_query, [status_seek]([[maybe_unused]]  int document_id, DocumentStatus status, int rating) { return status == status_seek; });
        
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
    template <typename Func>
    vector<Document> FindAllDocuments(const Query& query, Func predicat) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicat(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}



 template <typename T, typename U>
 void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                      const string& func, unsigned line, const string& hint) {
     if (t != u) {
         cout << boolalpha;
         cout << file << "("s << line << "): "s << func << ": "s;
         cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
         cout << t << " != "s << u << "."s;
         if (!hint.empty()) {
             cout << " Hint: "s << hint;
         }
         cout << endl;
         abort();
     }
 }

 #define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

 #define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

 void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                 const string& hint) {
     if (!value) {
         cout << file << "("s << line << "): "s << func << ": "s;
         cout << "ASSERT("s << expr_str << ") failed."s;
         if (!hint.empty()) {
             cout << " Hint: "s << hint;
         }
         cout << endl;
         abort();
     }
 }

 #define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

 #define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename t>
void RunTestImpl(t func, string func_s) {
    func();
    cerr<<func_s<<" OK"<<endl;
}

#define RUN_TEST(func)  RunTestImpl(func,#func)

// -------- Начало модульных тестов поисковой системы ----------


void TestAddDocument()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
 
    
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(),0);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(),1);
        const auto found_docs = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}
 


 

void TestMinusWords()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
 
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
 
    {
        SearchServer server;
        server.AddDocument(doc_id, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "cat at the city"s, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat -in"s).size(), 1);
        
        
    }
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-in"s).empty());
    }
 
}
 

void TestMatchedDocuments()
{
    {
        SearchServer server;
        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, -3 });
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id)
        {
            
            const auto& [words, status] = server.MatchDocument("fluffy cat"s, document_id);
            
            ASSERT_EQUAL(words.size(), 1);
            ASSERT_EQUAL(words[0], "cat"s);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
    
    {
        SearchServer server;
        server.SetStopWords("cat"s);
        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, -3});
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id)
        {
            
            const auto& [words, status] = server.MatchDocument("fluffy cat"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
 
    {
        SearchServer server;
        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, -3 });
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id)
        {
           
            const auto& [words, status] = server.MatchDocument("fluffy -cat"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
}
 

void TestSortFindedDocumentsByRelevance()
{
    {
        SearchServer server;

        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, { 8, -3 });
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        server.AddDocument(2, "soigne dog expressive eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2 });
        server.AddDocument(3, "soigne starling eugeny"s, DocumentStatus::BANNED, { 9 });
 
        const auto& documents = server.FindTopDocuments("fluffy soigne cat"s);
        if (documents.size() >= 1){
            
            for (int i = 1; i < documents.size(); ++i)
            {
                ASSERT(documents[i - 1].relevance > documents[i].relevance);
            }
        }else{
            abort();
        }
        
    }
 
   
}
 

void TestRatings()
{
    
    SearchServer server;
    vector<int> test_ratings = {10, 0, 5, 5};
    int test_ratings_mean = accumulate(test_ratings.begin(),test_ratings.end(), 0) / test_ratings.size();
    server.SetStopWords(" is are was a an in the with near at"s);
    server.AddDocument(0, "parrot parrot parrot"s, DocumentStatus::ACTUAL, test_ratings);
    
    const auto& documents = server.FindTopDocuments("parrot"s);
    vector<double> ratings;
    
    if (documents.size() == 1){
        ASSERT_EQUAL(documents[0].rating,test_ratings_mean);
        
    }else{
        abort();
    }
}


void TestRelevances()
{
        
        SearchServer server;
    
        server.SetStopWords(" cat cat cat"s);
        server.AddDocument(0, "dog dog dog"s, DocumentStatus::ACTUAL, {8, 2});
        server.AddDocument(1, "parrot parrot parrot"s, DocumentStatus::ACTUAL, {8, 2});
        
        const auto& documents = server.FindTopDocuments("parrot parrot parrot"s);
        vector<double> relevances;
        vector<double> ratings;
        
        for (const auto& doc: documents){
            relevances.push_back(doc.relevance);
           
        }
        ASSERT_EQUAL(relevances.size(), 1);
        ASSERT(abs(relevances[0] - log(server.GetDocumentCount() * 1.0 / 1)) < EPS);
        

        
    }

void TestSearchByStatus()
{
    SearchServer server;
    
    server.SetStopWords(" is are was a an in the with near at"s);
    server.AddDocument(0, "dog dog"s, DocumentStatus::ACTUAL, {8, 2});
    server.AddDocument(1, "dog"s, DocumentStatus::ACTUAL, {8, 2});
    server.AddDocument(2, "cat"s, DocumentStatus::BANNED, {8, 2});
    server.AddDocument(3, "parrot"s, DocumentStatus::IRRELEVANT, {8, 2});
    
    {
        const auto& documents = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(documents.size(), 0);
    }
    {
        const auto& documents = server.FindTopDocuments("dog"s);
        ASSERT_EQUAL(documents.size(), 2);
        
    }
    {
        const auto& documents = server.FindTopDocuments("parrot"s,DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents.size(), 1);
        
    }
    {
        const auto& documents = server.FindTopDocuments("parrot"s,DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 0);
        
    }
    
}
void TestSearchByPredicate()
{
    SearchServer server;
    
    server.SetStopWords(" is are was a an in the with near at"s);
    server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {5});
    server.AddDocument(2, "cat"s, DocumentStatus::ACTUAL, {10});
    
    {
        const auto& documents = server.FindTopDocuments("cat"s,[](int document_id, DocumentStatus status, double rating){
            return rating > 5;
        });
        ASSERT_EQUAL(documents.size(), 1);
    }
    {
        const auto& documents = server.FindTopDocuments("cat"s,[](int document_id, DocumentStatus status, double rating){
            return rating > 2;
        });
        ASSERT_EQUAL(documents.size(), 2);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}


void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchedDocuments);
    RUN_TEST(TestSortFindedDocumentsByRelevance);
    RUN_TEST(TestRatings);
    RUN_TEST(TestRelevances);
    RUN_TEST(TestSearchByStatus);
    RUN_TEST(TestSearchByPredicate);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}
