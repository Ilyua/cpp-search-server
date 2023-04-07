
#include "remove_duplicates.h"

using namespace std;
void RemoveDuplicates(SearchServer &search_server) {

    vector<int> documents_to_remove;

    map<set<string>, int> words2doc_id;
    set<string> words_in_doc;
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