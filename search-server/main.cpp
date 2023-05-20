// #include <algorithm>
// #include <cmath>
// #include <map>
// #include <set>
// #include <string>
// #include <utility>
// #include <vector>
// #include <iostream>
// #include <numeric>
// #include <deque>


#include "search_server.h"
#include "request_queue.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "process_queries.h"


#include <random>
#include "log_duration.h"

using namespace std;



template<typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
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

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint) {
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

template<typename t>
void RunTestImpl(t func, string func_s) {
    func();
    cerr << func_s << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server("и в на"s);
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto found_docs = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

void TestMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server("и в на"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("и в на"s);
        server.AddDocument(doc_id, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "cat at the city"s, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat -in"s).size(), 1);
    }

    {
        SearchServer server("и в на"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-in"s).empty());
    }
}

void TestMatchedDocuments() {
    {
        SearchServer server("и в на"s);
        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, -3});
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {

            const auto &[words, status] = server.MatchDocument("fluffy cat"s, document_id);

            ASSERT_EQUAL(words.size(), 1);
            ASSERT_EQUAL(words[0], "cat"s);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }

    {
        SearchServer server("cat"s);

        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, -3});
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {

            const auto &[words, status] = server.MatchDocument("fluffy cat"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }

    {
        SearchServer server("и в на"s);
        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, -3});
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {

            const auto &[words, status] = server.MatchDocument("fluffy -cat"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
}

void TestSortFindedDocumentsByRelevance() {
    {
        SearchServer server("и в на"s);

        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "soigne dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2});
        server.AddDocument(3, "soigne starling eugeny"s, DocumentStatus::BANNED, {9});

        const auto &documents = server.FindTopDocuments("fluffy soigne cat"s);

        for (unsigned int i = 1; i < documents.size(); ++i) {
            ASSERT(documents[i - 1].relevance > documents[i].relevance);
        }
    }
}

void TestRatings() {

    SearchServer server(" is are was a an in the with near at"s);
    vector<int> test_ratings = {10, 0, 5, 5};
    int test_ratings_mean = accumulate(test_ratings.begin(), test_ratings.end(), 0) / test_ratings.size();

    server.AddDocument(0, "parrot parrot parrot"s, DocumentStatus::ACTUAL, test_ratings);

    const auto &documents = server.FindTopDocuments("parrot"s);
    vector<double> ratings;

    ASSERT_EQUAL(documents.size(), 1);

    ASSERT_EQUAL(documents[0].rating, test_ratings_mean);
}

void TestRelevances() {

    SearchServer server(" cat cat cat"s);

    server.AddDocument(0, "dog dog dog"s, DocumentStatus::ACTUAL, {8, 2});
    server.AddDocument(1, "parrot parrot parrot"s, DocumentStatus::ACTUAL, {8, 2});

    const auto &documents = server.FindTopDocuments("parrot parrot parrot"s);
    vector<double> relevances;
    vector<double> ratings;

    for (const auto &doc: documents) {
        relevances.push_back(doc.relevance);
    }
    ASSERT_EQUAL(relevances.size(), 1);
    ASSERT(std::abs(relevances[0] - std::log(server.GetDocumentCount() * 1.0 / 1)) < EPS);
}

void TestSearchByStatus() {
    SearchServer server(" is are was a an in the with near at"s);

    server.AddDocument(0, "dog dog"s, DocumentStatus::ACTUAL, {8, 2});
    server.AddDocument(1, "dog"s, DocumentStatus::ACTUAL, {8, 2});
    server.AddDocument(2, "cat"s, DocumentStatus::BANNED, {8, 2});
    server.AddDocument(3, "parrot"s, DocumentStatus::IRRELEVANT, {8, 2});

    {
        const auto &documents = server.FindTopDocuments("parrot"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {
        const auto &documents = server.FindTopDocuments("parrot"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 0);
    }
}

void TestSearchByPredicate() {
    SearchServer server(" is are was a an in the with near at"s);

    server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {5});
    server.AddDocument(2, "cat"s, DocumentStatus::ACTUAL, {10});

    {
        const auto &documents = server.FindTopDocuments("cat"s, []([[maybe_unused]] int document_id,
                                                                   [[maybe_unused]] DocumentStatus status,
                                                                   double rating) { return rating > 5; });
        ASSERT_EQUAL(documents.size(), 1);
    }
    {
        const auto &documents = server.FindTopDocuments("cat"s, []([[maybe_unused]] int document_id,
                                                                   [[maybe_unused]] DocumentStatus status,
                                                                   double rating) { return rating > 2; });
        ASSERT_EQUAL(documents.size(), 2);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        {
            const auto found_docs = server.FindTopDocuments("in"s);
            ASSERT_EQUAL(found_docs.size(), 0);
        }
        {
            const auto found_docs = server.FindTopDocuments("cat"s);
            ASSERT_EQUAL(found_docs.size(), 1);
            const Document &doc0 = found_docs[0];
            ASSERT_EQUAL(doc0.id, doc_id);
        }
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
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


void RemoveDuplicates(SearchServer &search_server) {

    vector<int> documents_to_remove;

    map<set<string_view>, int> words2doc_id;
    set<string_view> words_in_doc;
    for (int document_id: search_server) {
        words_in_doc.clear();
        for (const auto [word, freqs]: search_server.GetWordFrequencies(document_id)) {
            words_in_doc.insert(word);
        }

        if (words2doc_id.count(words_in_doc) == 0) {
            words2doc_id[words_in_doc] = document_id;
        } else {

            documents_to_remove.push_back(document_id);
        }

    }
    for (int document_id: documents_to_remove) {
        cout << "Found duplicate document id " << document_id << endl;
        search_server.RemoveDocument(document_id);
    }
}

void AddDocument(SearchServer &search_server, int document_id, const string &document, DocumentStatus status,
                 const vector<int> &ratings) {
    search_server.AddDocument(document_id, document, status, ratings);

}
// --------- Окончание модульных тестов поисковой системы -----------


// ----------Добавляем параллельность-------------



string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, const string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << word_count << endl;
}

#define TEST(policy) Test(#policy, search_server, query, execution::policy)

int main() {
//    TestSearchServer();
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 10000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'00, 200);

    const string query = GenerateQuery(generator, dictionary, 500, 0.1);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TEST(seq);
    TEST(par);
}
//
//
//int main() {
//    SearchServer search_server("and with"s);
//    int id = 0;
//    for (
//        const string& text : {
//            "funny pet and nasty rat"s,
//            "funny pet with curly hair"s,
//            "funny pet and not very nasty rat"s,
//            "pet with rat and rat and rat"s,
//            "nasty rat with curly hair"s,
//    }
//            ) {
//        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
//    }
//    const vector<string> queries = {
//            "nasty rat -not"s,
//            "not very funny nasty pet"s,
//            "curly hair"s
//    };
//    id = 0;
//    for (
//        const auto& documents : ProcessQueries(search_server, queries)
//            ) {
//        cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
//    }
//    return 0;
//}

//int main() {
//    TestSearchServer();
//    SearchServer search_server("and with"s);
//
//    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
//    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // дубликат документа 2, будет удалён
//    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // отличие только в стоп-словах, считаем дубликатом
//    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // множество слов такое же, считаем дубликатом документа 1
//    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // добавились новые слова, дубликатом не является
//    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
//    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // есть не все слова, не является дубликатом
//    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});
//
//    // слова из разных документов, не является дубликатом
//    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
//    RemoveDuplicates(search_server);
//    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
//}
//int main()
//{
//    TestSearchServer();
//    SearchServer search_server("and in at"s);
//    RequestQueue request_queue(search_server);
//    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
//    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
//    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
//    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
//    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
//    // 1439 запросов с нулевым результатом
//    for (int i = 0; i < 1439; ++i)
//    {
//        request_queue.AddFindRequest("empty request"s);
//    }
//    // все еще 1439 запросов с нулевым результатом
//    request_queue.AddFindRequest("curly dog"s);
//    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
//    request_queue.AddFindRequest("big collar"s);
//    // первый запрос удален, 1437 запросов с нулевым результатом
//    request_queue.AddFindRequest("sparrow"s);
//    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
//    return 0;
//}
