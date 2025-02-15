![cpp-sort logo](docs/images/cpp-sort-logo.svg)

[![Latest Release](https://img.shields.io/badge/release-1.14.0-blue.svg)](https://github.com/Morwenn/cpp-sort/releases/tag/1.14.0)
[![Conan Package](https://img.shields.io/badge/conan-cpp--sort%2F1.14.0-blue.svg)](https://conan.io/center/cpp-sort?version=1.14.0)
[![Code Coverage](https://codecov.io/gh/Morwenn/cpp-sort/branch/develop/graph/badge.svg)](https://codecov.io/gh/Morwenn/cpp-sort)
[![Pitchfork Layout](https://img.shields.io/badge/standard-PFL-orange.svg)](https://github.com/vector-of-bool/pitchfork)

> *It would be nice if only one or two of the sorting methods would dominate all of the others,
> regardless of application or the computer being used. But in fact, each method has its own
> peculiar virtues. [...] Thus we find that nearly all of the algorithms deserve to be remembered,
> since there are some applications in which they turn out to be best.*
> — Donald Knuth, The Art Of Computer Programming, Volume 3

**cpp-sort** is a generic C++14 header-only sorting library. It revolves
around one main generic sorting interface and provides several small tools
to pick and/or design sorting algorithms. Using its basic sorting features
should be trivial enough:

```cpp
#include <array>
#include <iostream>
#include <cpp-sort/sorters/smooth_sorter.h>

int main()
{
    std::array<int, 5> arr = { 5, 8, 3, 2, 9 };
    cppsort::smooth_sort(arr);

    // prints 2 3 5 8 9
    for (int val: arr) {
        std::cout << val << ' ';
    }
}
```

# The main features & the extra features

**cpp-sort** provides a full set of sorting-related features. Here are the main building blocks
of the library:
* Every sorting algorithm exists as a function object called a [sorter](https://github.com/Morwenn/cpp-sort/wiki/Sorters)
* Sorters can be wrapped in [sorter adapters](https://github.com/Morwenn/cpp-sort/wiki/Sorter-adapters) to augment their behaviour
* The library provides a [sorter facade](https://github.com/Morwenn/cpp-sort/wiki/Sorter-facade) to easily build sorters
* [Fixed-size sorters](https://github.com/Morwenn/cpp-sort/wiki/Fixed-size-sorters) can be used to efficiently sort tiny fixed-size collections
* [Measures of presortedness](https://github.com/Morwenn/cpp-sort/wiki/Measures-of-presortedness) can be used to evaluate the disorder in a collection

Here is a more complete example of what can be done with the library:

```cpp
#include <algorithm>
#include <cassert>
#include <forward_list>
#include <functional>
#include <vector>
#include <cpp-sort/adapters.h>
#include <cpp-sort/sorters.h>

int main()
{
    struct wrapper { int value; };

    std::forward_list<wrapper> li = { {5}, {8}, {3}, {2}, {9} };
    std::vector<wrapper> vec = { {5}, {8}, {3}, {2}, {9} };

    // When used, this sorter will use a pattern-defeating quicksort
    // to sort random-access collections, and a mergesort otherwise
    cppsort::hybrid_adapter<
        cppsort::pdq_sorter,
        cppsort::merge_sorter
    > sorter;

    // Sort li and vec in reverse order using their value member
    sorter(li, std::greater<>{}, &wrapper::value);
    sorter(vec, std::greater<>{}, &wrapper::value);

    assert(std::equal(
        li.begin(), li.end(),
        vec.begin(), vec.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.value == rhs.value; }
    ));
}
```

Even when the sorting functions are used without the extra features, they still provide
some interesting guarantees (ideas often taken from the Ranges TS):
* They provide both an iterator and a range interface
* When possible, they accept a custom comparator parameter
* Most of them accept a projection parameter
* They correctly handle proxy iterators with `iter_swap` and `iter_move`
* They also work when iterators don't provide post-incrementation nor post-decrementation
* The value types of the collections to be sorted need not be default-constructible
* The value types of the collections to be sorted need not be copyable (only movable)
* Stateless sorters can be converted to a function pointer for each overloaded `operator()`
* Sorters are function objects: they can directly be passed as "overload sets" to other functions

You can read more about all the available tools and find some tutorials about using
and extending **cpp-sort** in [the wiki](https://github.com/Morwenn/cpp-sort/wiki).

# Benchmarks

The following graph has been generated with a script found in the benchmarks
directory. It shows the time needed for [`heap_sort`][heap-sorter] to sort one
million elements without being adapted, then when it is adapted with either
[`drop_merge_adapter`][drop-merge-adapter] or [`split_adapter`][split-adapter].

![Graph showing the speed difference between heap_sort raw, then adapted with
split_adapter and drop_merge_adapter, when the number of inversions in the
std::vector<int> to sort increases](https://i.imgur.com/IcjUkYF.png)

As can be seen above, wrapping `heap_sort` with either of the adapters makes it
[*adaptive*][adaptive-sort] to the number of inversions in a non-intrusive
manner. The algorithms used to adapt it have different pros and cons, it is up
to you to use either.

This benchmark is mostly there to show the possibilities offered by the
library. You can find more such commented benchmarks in the [dedicated wiki
page][benchmarks].

# Compiler support & tooling

![Ubuntu builds status](https://github.com/Morwenn/cpp-sort/workflows/Ubuntu%20Builds/badge.svg?branch=develop)
![Windows builds status](https://github.com/Morwenn/cpp-sort/workflows/MSVC%20Builds/badge.svg?branch=develop)
![MacOS builds status](https://github.com/Morwenn/cpp-sort/workflows/MacOS%20Builds/badge.svg?branch=develop)

**cpp-sort** requires C++14 support, and should work with the following compilers:
* g++7 or more recent.
* clang++6.0 or more recent (with both libstdc++ and libc++).
* The versions of MinGW-w64 and AppleClang equivalent to the compilers mentioned above.
* Visual Studio 2019 version 16.8.3 or more recent, only with `/permissive-`. A few features are unavailable.
* clang-cl corresponding the the Visual Studio version above.

The compilers listed above are the ones used by the CI pipeline, and the library is also tested
with the most recent versions of those compilers on a regular basis. All the other compiler
versions in-between are untested, but should also work. Feel free to open an issue if it isn't the
case.

The features in the library might differ depending on the C++ version used and on the compiler
extensions enabled. Those changes are documented [in the wiki][changelog].

The main repository contains additional support for standard tooling such as CMake or Conan.
You can read more about those [in the wiki][tooling].

# Thanks

> *I got a new car. I just need to put it together. They’re easier to steal piece by
> piece.*
> — Jarod Kintz, $3.33

Even though some parts of the library are [original research](https://github.com/Morwenn/cpp-sort/wiki/Original-research)
and some others correspond to custom and rather naive implementations of standard
sorting algorithms, **cpp-sort** also reuses a great deal of code and ideas from
open-source projects, often altered to integrate seamlessly into the library. Here
is a list of the external resources used to create this library. I hope that the
many different licenses are compatible. If it is not the case, please contact me
(or submit an issue) and we will see what can be done about it:

* Some of the algorithms used by `insertion_sorter` and `pdq_sorter` come from
Orson Peters' [pattern-defeating quicksort](https://github.com/orlp/pdqsort). Some
parts of the benchmarks come from there as well.

* The algorithm used by `tim_sorter` comes from Goro Fuji's (gfx) [implementation
of a Timsort](https://github.com/gfx/cpp-TimSort).

* The three algorithms used by `spread_sorter` come from Steven Ross [Boost.Sort
module](https://www.boost.org/doc/libs/1_80_0/libs/sort/doc/html/index.html).

* The algorithm used by `d_ary_spread_sorter` comes from Tim Blechmann's
[Boost.Heap module](https://www.boost.org/doc/libs/1_80_0/doc/html/heap.html).

* The algorithm used by `spin_sorter` comes from the eponymous algorithm implemented
in [Boost.Sort](https://www.boost.org/doc/libs/1_80_0/libs/sort/doc/html/index.html).
by Francisco Jose Tapia.

* [`utility::as_function`](https://github.com/Morwenn/cpp-sort/wiki/Miscellaneous-utilities#as_function),
[`utility::static_const`](https://github.com/Morwenn/cpp-sort/wiki/Miscellaneous-utilities#static_const),
and several projection-enhanced helper algorithms come from Eric Niebler's [Range
v3](https://github.com/ericniebler/range-v3) library. Several ideas such as proxy
iterators, customization points and projections, as well as a few other utility
functions also come from that library or from the related articles and standard
C++ proposals.

* The algorithm used by `ska_sorter` comes from Malte Skarupke's [implementation](https://github.com/skarupke/ska_sort)
of his own [ska_sort](https://probablydance.com/2016/12/27/i-wrote-a-faster-sorting-algorithm/) algorithm.

* The algorithm used by `drop_merge_sorter` comes from Adrian Wielgosik [C++
reimplementation](https://github.com/adrian17/cpp-drop-merge-sort) of Emil Ernerfeldt's
[drop-merge sort](https://github.com/emilk/drop-merge-sort).

* Many enhanced standard algorithms are directly adapted from their counterparts
in [libc++](https://libcxx.llvm.org/), enhanced to handle both projections and
proxy iterators.

* The library internally uses an `inplace_merge` function that works with forward
iterators. Its implementation uses a merge algorithm proposed by Dudziński and Dydek,
and implemented by Alexander Stepanov and Paul McJones in their book [*Elements of
Programming*](http://www.elementsofprogramming.com/).

* The `inplace_merge` overload for random-access iterators uses the *Symmerge* algorithm
proposed by Pok-Son Kim and Arne Kutzner in [*Stable Minimum Storage Merging by Symmetric
Comparisons*](https://pdfs.semanticscholar.org/d664/cee462cb8e6a8ae2a1a7c6bab1b5f81e0618.pdf)
when there isn't enough memory available to perform an out-of-place merge.

* The implementation of Dijkstra's smoothsort used by `smooth_sorter` has been
directly adapted from [Keith Schwarz's implementation](http://www.keithschwarz.com/interesting/code/?dir=smoothsort)
of the algorithm.

* The algorithm used by `wiki_sorter` has been adapted from BonzaiThePenguin's
[WikiSort](https://github.com/BonzaiThePenguin/WikiSort).

* The algorithm used by `grail_sorter` has been adapted from Mrrl's
[GrailSort](https://github.com/Mrrl/GrailSort).

* The algorithm used by `indirect_adapter` with forward or bidirectional iterators is a
slightly modified version of Matthew Bentley's [indiesort](https://github.com/mattreecebentley/plf_indiesort).

* The implementation of the random-access overload of `nth_element` used by some of the algorithms
comes from Danila Kutenin's [miniselect library](https://github.com/danlark1/miniselect) and uses
Andrei Alexandrescu's [*AdaptiveQuickselect*](https://arxiv.org/abs/1606.00484) algorithm.

* The sorting networks used by `sorting_network_sorter` all come [from this list](http://users.telenet.be/bertdobbelaere/SorterHunter/sorting_networks.html)
maintained by Bert Dobbelaere. The page has references to the sources of all of the sorting networks
it lists.

* Some of the optimizations used by `sorting_network_sorter` come from [this
discussion](https://stackoverflow.com/q/2786899/1364752) on StackOverflow and are
backed by the article [*Applying Sorting Networks to Synthesize Optimized Sorting
Libraries*](https://arxiv.org/abs/1505.01962).

* The test suite reimplements random number algorithms originally found in the following places:
  - [xoshiro256\*\*](https://prng.di.unimi.it/)
  - [*Optimal Discrete Uniform Generation from Coin Flips, and Applications*](https://arxiv.org/abs/1304.1916)
  - [*All numbers in a given range but random order*](https://stackoverflow.com/a/44821946/1364752)

* The LaTeX scripts used to draw the sorting networks are modified versions of
kaayy's [`sortingnetwork.tex`](https://github.com/kaayy/kaayy-s-code-sinppets),
slightly adapted to be 0-based and draw the network from top to bottom.

* The CMake tools embedded in the projects include scripts from [RWTH-HPC/CMake-codecov](https://github.com/RWTH-HPC/CMake-codecov)
and [Crascit/DownloadProject](https://github.com/Crascit/DownloadProject).

* Some of the benchmarks use a [colorblind-friendly palette](https://gist.github.com/thriveth/8560036)
developed by Thøger Rivera-Thorsen.


  [adaptive-sort]: https://en.wikipedia.org/wiki/Adaptive_sort
  [benchmarks]: https://github.com/Morwenn/cpp-sort/wiki/Benchmarks
  [changelog]: https://github.com/Morwenn/cpp-sort/wiki/Changelog
  [drop-merge-adapter]: https://github.com/Morwenn/cpp-sort/wiki/Sorter-adapters#drop_merge_adapter
  [heap-sorter]: https://github.com/Morwenn/cpp-sort/wiki/Sorters#heap_sorter
  [split-adapter]: https://github.com/Morwenn/cpp-sort/wiki/Sorter-adapters#split_adapter
  [tooling]: https://github.com/Morwenn/cpp-sort/wiki/Tooling
