/*
 * Copyright (c) 2016-2020 Morwenn
 * SPDX-License-Identifier: MIT
 */
#ifndef CPPSORT_PROBES_RUNS_H_
#define CPPSORT_PROBES_RUNS_H_

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <functional>
#include <iterator>
#include <type_traits>
#include <cpp-sort/sorter_facade.h>
#include <cpp-sort/sorter_traits.h>
#include <cpp-sort/utility/as_function.h>
#include "../detail/iterator_traits.h"

namespace cppsort::probe
{
    namespace detail
    {
        struct runs_impl
        {
            template<
                typename ForwardIterator,
                typename Compare = std::less<>,
                typename Projection = std::identity,
                typename = std::enable_if_t<
                    is_projection_iterator_v<Projection, ForwardIterator, Compare>
                >
            >
            auto operator()(ForwardIterator first, ForwardIterator last,
                            Compare compare={}, Projection projection={}) const
                -> cppsort::detail::difference_type_t<ForwardIterator>
            {
                using difference_type = cppsort::detail::difference_type_t<ForwardIterator>;
                auto&& comp = utility::as_function(compare);
                auto&& proj = utility::as_function(projection);

                if (first == last || std::next(first) == last) {
                    return 0;
                }

                auto current = first;
                auto next = std::next(first);

                difference_type count = 0;
                while (true) {
                    while (next != last && not comp(proj(*next), proj(*current))) {
                        ++current;
                        ++next;
                    }

                    if (next == last) break;
                    ++count;
                    ++current;
                    ++next;
                }
                return count;
            }
        };
    }

    inline constexpr sorter_facade<detail::runs_impl> runs{};
}

#endif // CPPSORT_PROBES_RUNS_H_
