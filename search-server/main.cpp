#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text) {
    vector<string> words;
    string word;
    for (const char c: text) {
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
};

class SearchServer {
public:
    void SetStopWords(const string &text) {
        for (const string &word: SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        map <string,int> word_count_buf;
        if (!words.empty()) { document_count_++; }
        for (const string &word: words) {
            word_count_buf[word] += 1;
            term_frequency_[word].insert({
                                                 document_id,
                                                 word_count_buf[word] /  static_cast<double>(words.size())
                                         });

        }
    }

    vector<Document> FindTopDocuments(const string &raw_query) const {
        Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    int document_count_ = 0;

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, map<int, double>> term_frequency_;

    set<string> stop_words_;

    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const string &word: SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string &text) const {
        Query query;
        for (const string &word: SplitIntoWordsNoStop(text)) {
            if (word[0] != '-') {
                query.plus_words.insert(word);
            } else {
                query.minus_words.insert(word.substr(1));
            }

        }
        for (string word: query.minus_words) {
            query.plus_words.erase(word);

        }
        return query;
    }
    double CalcIDF(string word) const{
        return log(static_cast<double> (document_count_) / term_frequency_.at(word).size());
    }
    vector<Document> FindAllDocuments(Query &query) const {
        map<int, double> document_relevance_buf;
        vector<Document> matched_documents;


        for (string word: query.plus_words) {
            if (term_frequency_.count(word) == 0) {
                continue;
            }

            double word_idf = CalcIDF(word);
            for (auto [document_id, tf]: term_frequency_.at(word)) {
                document_relevance_buf[document_id] += word_idf * tf;
            }

        }

        for (auto [document_id, relevance]: document_relevance_buf) {
            bool toInsert = true;

            for (string word: query.minus_words) {
                if (term_frequency_.count(word) == 0) {
                    continue;
                }
                if (term_frequency_.at(word).count(document_id) != 0) {
                    toInsert = false;
                }
            }
            if (toInsert) {
                matched_documents.push_back({document_id, relevance});
            }

        }


        return matched_documents;
    }


};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto &[document_id, relevance]: search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}