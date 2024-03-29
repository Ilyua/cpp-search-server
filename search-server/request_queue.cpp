#include "request_queue.h"


RequestQueue::RequestQueue(const SearchServer &search_server) : server_(search_server)
{
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentStatus status)
{
    std::vector<Document> temp = server_.FindTopDocuments(raw_query, status);
    QueryResult temp_query;
    temp_query.count_documents = temp.size();
    AddQueryResult(temp_query);
    return temp;
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query)
{
    std::vector<Document> temp = server_.FindTopDocuments(raw_query);
    QueryResult temp_query;
    temp_query.count_documents = temp.size();
    AddQueryResult(temp_query);
    return temp;
}

int RequestQueue::GetNoResultRequests() const
{
    return count_;
}

void RequestQueue::AddQueryResult(const QueryResult &query)
{
    if (requests_.size() >= min_in_day_)
    {
        QueryResult results = requests_.front();
        requests_.pop_front();
        if (results.count_documents == 0)
        {
            count_--;
        }
    }
    if (query.count_documents == 0)
    {
        count_++;
    }
    requests_.push_back(query);
}


