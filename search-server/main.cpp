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


using namespace std;

// ==================== для примера =========================

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint)
{
    if (t != u)
    {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty())
        {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint)
{
    if (!value)
    {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty())
        {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename t>
void RunTestImpl(t func, string func_s)
{
    func();
    cerr << func_s << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestAddDocument()
{
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

void TestMinusWords()
{
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

void TestMatchedDocuments()
{
    {
        SearchServer server("и в на"s);
        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, -3});
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id)
        {

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
        for (int document_id = 0; document_id < document_count; ++document_id)
        {

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
        for (int document_id = 0; document_id < document_count; ++document_id)
        {

            const auto &[words, status] = server.MatchDocument("fluffy -cat"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
}

void TestSortFindedDocumentsByRelevance()
{
    {
        SearchServer server("и в на"s);

        server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(2, "soigne dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2});
        server.AddDocument(3, "soigne starling eugeny"s, DocumentStatus::BANNED, {9});

        const auto &documents = server.FindTopDocuments("fluffy soigne cat"s);

        for (unsigned int i = 1; i < documents.size(); ++i)
        {
            ASSERT(documents[i - 1].relevance > documents[i].relevance);
        }
    }
}

void TestRatings()
{

    SearchServer server(" is are was a an in the with near at"s);
    vector<int> test_ratings = {10, 0, 5, 5};
    int test_ratings_mean = accumulate(test_ratings.begin(), test_ratings.end(), 0) / test_ratings.size();

    server.AddDocument(0, "parrot parrot parrot"s, DocumentStatus::ACTUAL, test_ratings);

    const auto &documents = server.FindTopDocuments("parrot"s);
    vector<double> ratings;

    ASSERT_EQUAL(documents.size(), 1);

    ASSERT_EQUAL(documents[0].rating, test_ratings_mean);
}

void TestRelevances()
{

    SearchServer server(" cat cat cat"s);

    server.AddDocument(0, "dog dog dog"s, DocumentStatus::ACTUAL, {8, 2});
    server.AddDocument(1, "parrot parrot parrot"s, DocumentStatus::ACTUAL, {8, 2});

    const auto &documents = server.FindTopDocuments("parrot parrot parrot"s);
    vector<double> relevances;
    vector<double> ratings;

    for (const auto &doc : documents)
    {
        relevances.push_back(doc.relevance);
    }
    ASSERT_EQUAL(relevances.size(), 1);
    ASSERT(std::abs(relevances[0] - std::log(server.GetDocumentCount() * 1.0 / 1)) < EPS);
}

void TestSearchByStatus()
{
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
void TestSearchByPredicate()
{
    SearchServer server(" is are was a an in the with near at"s);

    server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {5});
    server.AddDocument(2, "cat"s, DocumentStatus::ACTUAL, {10});

    {
        const auto &documents = server.FindTopDocuments("cat"s, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, double rating)
                                                        { return rating > 5; });
        ASSERT_EQUAL(documents.size(), 1);
    }
    {
        const auto &documents = server.FindTopDocuments("cat"s, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, double rating)
                                                        { return rating > 2; });
        ASSERT_EQUAL(documents.size(), 2);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent()
{
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
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestSearchServer()
{
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
int main()
{
    TestSearchServer();
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i)
    {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}
