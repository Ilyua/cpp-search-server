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

struct DocumentData
{
    int rating;
    DocumentStatus status;
};

std::ostream &operator<<(std::ostream &out, const DocumentStatus &status);

std::ostream &operator<<(std::ostream &out, const Document &doc);

void PrintDocument(const Document &document);