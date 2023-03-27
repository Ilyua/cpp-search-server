#pragma once

#include <iostream>
#include <string>

using namespace std::literals::string_literals;

struct Document
{
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id), relevance(relevance), rating(rating)
    {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};



enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

std::ostream &operator<<(std::ostream &out, const DocumentStatus &status)
{

    out << static_cast<int>(status);

    return out;
}

std::ostream &operator<<(std::ostream &out, const Document &doc)
{
    out << "{ "s
        << "document_id = "s << doc.id << ", "s
        << "relevance = "s << doc.relevance << ", "s
        << "rating = "s << doc.rating << " }"s;
    return out;
}

void PrintDocument(const Document &document)
{
    std::cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << std::endl;
}
