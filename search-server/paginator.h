#pragma once // помогает так же от двойной реализации

#include <iostream>
#include <vector>



template <typename It>
struct IteratorRange
{
    It begin;
    It end;

    int size()
    {
        return end - begin;
    }
};



template <typename It>
class Paginator
{
public:
    std::vector<IteratorRange<It>> pages;
    size_t step;
    size_t diff;
    explicit Paginator(It begin, It end, size_t size)
    {
        It iter = begin;
        while (std::distance(iter, end) > 0)
        {
            diff = std::distance(iter, end);
            step = std::min(diff, size);
            auto new_begin = iter;
            std::advance(iter, step);

            IteratorRange<It> range = {new_begin,
                                       iter};
            pages.push_back(range);
        }
    }

    auto begin() const
    {
        return pages.begin();
    }

    auto end() const
    {
        return pages.end();
    }
};


template <typename Iterator>
std::ostream &operator<<(std::ostream &out, const IteratorRange<Iterator> &It)
{
    for (auto i = It.begin; i != It.end; ++i)
        out << *i;
    return out;
}

template <typename Container>
auto Paginate(const Container &c, size_t page_size)
{
    Paginator p(begin(c), end(c), page_size);
    return p;
}