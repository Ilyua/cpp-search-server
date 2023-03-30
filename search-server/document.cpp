#include "document.h"

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