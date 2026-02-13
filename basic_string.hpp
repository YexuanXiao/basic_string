// Copyright 2023-2025 YexuanXiao
// Distributed under the MIT License.
// https://github.com/YexuanXiao/basic_string

#if !defined(BIZWEN_BASIC_STRING_HPP)
#define BIZWEN_BASIC_STRING_HPP

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <functional>
#include <iterator>
#include <memory>
#include <memory_resource>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <version>

// if consteval
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 5282)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++23-extensions"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++23-extensions"
#endif

namespace bizwen
{

template <typename CharT, typename Traits = ::std::char_traits<CharT>, typename Alloc = ::std::allocator<CharT>>
class basic_string;

namespace string_detail
{
template <typename T, typename A>
class string_iterator
{
    using CharT = ::std::remove_const_t<T>;
    using Traits = typename ::std::char_traits<CharT>;
    friend class ::bizwen::basic_string<CharT, Traits, A>;
    friend string_iterator<T const, A>;
    friend string_iterator<CharT, A>;

  public:
    using difference_type = typename ::std::allocator_traits<A>::difference_type;
    using value_type = CharT;
    using pointer = T *;
    using reference = T &;
    using iterator_category = ::std::random_access_iterator_tag;
    using iterator_concept = ::std::contiguous_iterator_tag;

  private:
    pointer current_{};

#if !defined(NDEBUG)
    CharT const *begin_{};
    CharT const *end_{};
#endif

#if !defined(NDEBUG)
    // Call after iteration.
    constexpr bool verify() const noexcept
    {
        assert(current_ >= begin_ && current_ <= end_);
        return true;
    }
#endif

  public:
    string_iterator() noexcept = default;
    string_iterator(string_iterator const &) noexcept = default;
    string_iterator &operator=(string_iterator const &) & noexcept = default;

  private:
#if !defined(NDEBUG)
    constexpr string_iterator(T *current, CharT const *begin, CharT const *end) noexcept
        : current_(current), begin_(begin), end_(end)
    {
    }
#else
    constexpr string_iterator(T *current) noexcept : current_(current)
    {
    }
#endif

  public:
    constexpr string_iterator operator+(difference_type n) const & noexcept
    {
        auto temp = *this;
        temp.current_ += n;
        assert(temp.verify());

        return temp;
    }

    constexpr string_iterator operator-(difference_type n) const & noexcept
    {
        auto temp = *this;
        temp.current_ -= n;
        assert(temp.verify());

        return temp;
    }

    constexpr friend string_iterator operator+(difference_type n, string_iterator const &rhs) noexcept
    {
        auto temp = rhs;
        temp.current_ += n;
        assert(temp.verify());

        return temp;
    }

    constexpr friend string_iterator operator-(difference_type n, string_iterator const &rhs) noexcept
    {
        auto temp = rhs;
        temp.current_ -= n;
        assert(temp.verify());

        return temp;
    }

    constexpr friend difference_type operator-(string_iterator const &lhs, string_iterator const &rhs) noexcept
    {
#if !defined(NDEBUG)
        assert(lhs.begin_ == rhs.begin_);
#endif
        return lhs.current_ - rhs.current_;
    }

    constexpr string_iterator &operator+=(difference_type n) & noexcept
    {
        current_ += n;
        assert(verify());

        return *this;
    }

    constexpr string_iterator &operator-=(difference_type n) & noexcept
    {
        current_ -= n;
        assert(verify());

        return *this;
    }

    constexpr string_iterator &operator++() & noexcept
    {
        ++current_;
        assert(verify());

        return *this;
    }

    constexpr string_iterator &operator--() & noexcept
    {
        --current_;
        assert(verify());

        return *this;
    }

    constexpr string_iterator operator++(int) & noexcept
    {
        auto temp = *this;
        ++current_;
        assert(verify());

        return temp;
    }

    constexpr string_iterator operator--(int) & noexcept
    {
        auto temp = *this;
        --current_;
        assert(verify());

        return temp;
    }

    constexpr T &operator[](difference_type n) const noexcept
    {
#if !defined(NDEBUG)
        auto end = (*this) + n;
        assert(end.verify());
#endif
        return *(current_ + n);
    }

    constexpr auto &operator*() const noexcept
    {
        return *current_;
    }

    constexpr auto *operator->() const noexcept
    {
        return current_;
    }

#if !defined(NDEBUG)
    friend constexpr ::std::strong_ordering operator<=>(string_iterator const &lhs, string_iterator const &rhs) noexcept
    {
        assert(lhs.begin_ == rhs.begin_);
        return lhs.current_ <=> rhs.current_;
    }

    friend constexpr bool operator==(string_iterator const &lhs, string_iterator const &rhs) noexcept
    {
        assert(lhs.begin_ == rhs.begin_);
        return lhs.current_ == rhs.current_;
    }
#else
    friend constexpr ::std::strong_ordering operator<=>(string_iterator const &,
                                                        string_iterator const &) noexcept = default;
#endif

    constexpr operator string_iterator<CharT const, A>() const
        requires(!::std::is_const_v<T>)
    {
#if !defined(NDEBUG)
        return {current_, begin_, end_};
#else
        return {current_};
#endif
    }
};

template <typename A>
concept mini_alloc = requires(A &a) {
    typename A::value_type;
    a.allocate(0uz);
};
} // namespace string_detail

template <typename CharT, typename Traits, typename Alloc>
class basic_string
{
    static_assert(!::std::is_array_v<CharT>, "Character type of basic_string must not be an array");
    static_assert(::std::is_trivially_copyable_v<CharT>,
                  "Character type of basic_string must be trivially copyable");
    static_assert(::std::is_trivially_default_constructible_v<CharT>,
                  "Character type of basic_string must be trivially default constructible");
    static_assert(::std::is_standard_layout_v<CharT>, "Character type of basic_string must be standard-layout");
    static_assert(::std::is_same_v<CharT, typename Traits::char_type>,
                  "Traits::char_type must be the same type as CharT");
    static_assert(::std::is_same_v<CharT, typename Alloc::value_type>,
                  "Allocator::value_type must be the same type as CharT");

    using atraits_t_ = ::std::allocator_traits<Alloc>;

    template <typename StringViewLike>
    inline static constexpr bool is_svlike_v_ =
        ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<CharT, Traits>> &&
        (!::std::is_convertible_v<StringViewLike const &, CharT const *>);

  public:
    using traits_type = Traits;
    using value_type = CharT;
    using allocator_type = Alloc;
    using size_type = typename atraits_t_::size_type;
    using difference_type = typename atraits_t_::difference_type;
    using reference = value_type &;
    using const_reference = value_type const &;
    using pointer = typename atraits_t_::pointer;
    using const_pointer = typename atraits_t_::const_pointer;

    static inline constexpr size_type npos = size_type(-1);

  private:
    /**
     * @brief type of long string
     */
    struct ls_type_
    {
        pointer begin_{};
        CharT *end_{};
        CharT *last_{};

        constexpr auto begin() const noexcept
        {
            return ::std::to_address(begin_);
        }

        constexpr auto end() const noexcept
        {
            return ::std::to_address(end_);
        }
    };

    // -2 is due to the null terminator and size_flag
    /**
     * @brief short_str_max_ is the max length of short string
     */
    static inline constexpr ::std::size_t short_str_max_{std::max(sizeof(CharT *) * 3uz / sizeof(CharT), 4uz) - 2uz};

    struct ss_type_
    {
        CharT buffer_[short_str_max_ + ::std::size_t(1) /* null terminator */]{};

        constexpr auto data() const noexcept
        {
            return ::std::begin(buffer_);
        }

        constexpr auto data() noexcept
        {
            return ::std::begin(buffer_);
        }
    };

    /**
     * @brief union storage long string and short string
     */
#pragma pack(push, 1)
    union storage_type_ {
        ss_type_ ss_;
        ls_type_ ls_;
    };
#pragma pack(pop)
    // https://github.com/microsoft/STL/issues/1364
#if __has_cpp_attribute(msvc::no_unique_address)
    [[msvc::no_unique_address]] alignas(CharT *) Alloc allocator_{};
#else
    // Using the alignment of the allocator to ensure the entire structure is properly aligned
    [[no_unique_address]] alignas(CharT *) Alloc allocator_{};
#endif

    /**
     * @brief storage of string
     */
    storage_type_ stor_{};

    /**
     * @brief flag > 0: short string, length of string is size_flag
     * @brief flag = 0: empty string
     * @brief flag = MAX: long string, length of string is end - begin
     * @brief If the string migrates from short to long, then any operation other than
     * @brief move and shrink_to_fit will not make it revert back to short
     */
    alignas(CharT) unsigned char size_flag_{};

    // sizeof(basic_string<T, ::std::allocator>) is always equal to sizeof(void*) * 4

    [[noreturn]] static constexpr void throw_out_of_range_()
    {
        throw ::std::out_of_range("pos/index is out of range, please check it.");
    }

    [[noreturn]] static constexpr void throw_length_error_()
    {
        throw ::std::length_error("string too long.");
    }

    /**
     * @brief check if the two ranges overlap, and if overlap, new memory needs to
     * allocate
     **/
    static constexpr bool overlap_([[maybe_unused]] CharT const *first_inner, [[maybe_unused]] CharT const *last_inner,
                                   [[maybe_unused]] CharT const *first_outer,
                                   [[maybe_unused]] CharT const *last_outer) noexcept
    {
        // [utilities.comparisons.general] strict total order over pointers.
        if (::std::less{}(first_inner, first_outer) || !::std::less{}(first_inner, last_outer))
        {
            assert(::std::greater{}(first_outer, last_inner) || !::std::less{}(last_inner, last_outer));
            // input range overlaps with *this but is larger
            return false;
        }
        return true;
    }

    constexpr auto &long_str_() noexcept
    {
        return stor_.ls_;
    }

    constexpr auto &short_str_() noexcept
    {
        return stor_.ss_;
    }

    constexpr auto &long_str_() const noexcept
    {
        return stor_.ls_;
    }

    constexpr auto &short_str_() const noexcept
    {
        return stor_.ss_;
    }

    /**
     * @brief convert to long string and set the flag
     */
    constexpr void long_str_(ls_type_ const &ls) noexcept
    {
        stor_ = storage_type_{.ls_ = ls};
        size_flag_ = static_cast<unsigned char>(-1);
        *ls.end() = CharT{};
    }

    /**
     * @brief convert to short string and resize
     */
    constexpr void short_str_(size_type size) noexcept
    {
        stor_ = storage_type_{};
        size_flag_ = static_cast<unsigned char>(size);
        *(short_str_().data() + size) = CharT{};
    }

    constexpr bool is_long_() const noexcept
    {
        return size_flag_ == static_cast<unsigned char>(-1);
    }

    constexpr bool is_short_() const noexcept
    {
        return !is_long_();
    }

    constexpr void resize_shrink_(bool is_long, size_type n) noexcept
    {
        assert(is_long_() == is_long);
        if (is_long)
        {
            auto &ls = long_str_();
            ls.end_ = ls.begin() + n;
            *ls.end_ = CharT{};
        }
        else
        {
            size_flag_ = static_cast<unsigned char>(n);
            *(short_str_().data() + size_flag_) = CharT{};
        }
    }

    constexpr size_type size_() const noexcept
    {
        return is_short_() ? size_flag_ : long_str_().end() - long_str_().begin();
    }

    constexpr ::std::basic_string_view<value_type, traits_type> view_() const noexcept
    {
        return {begin_(), end_()};
    }

    template <class Ordering>
    static constexpr int order_to_int_(Ordering ord) noexcept
    {
        return ord < 0 ? -1 : (ord > 0 ? 1 : 0);
    }

    // ***************************** begin volume *****************************
  public:
    constexpr size_type size() const noexcept
    {
        return size_();
    }

    constexpr size_type length() const noexcept
    {
        return size_();
    }

    constexpr bool empty() const noexcept
    {
        return !size_();
    }

    /**
     * Useless function, its existence is only to pass pedantic tests, using it is just a waste of time
     */
    constexpr size_type max_size() const noexcept
    {
        return ::std::min(size_type(-1) / size_type(sizeof(CharT)) / size_type(2),
                          static_cast<size_type>(atraits_t_::max_size(allocator_))) -
               size_type(1) /* null terminator */;
    }

    constexpr size_type capacity() const noexcept
    {
        return is_short_() ? short_str_max_ : long_str_().last_ - long_str_().begin();
    }

    /**
     * @brief shrink only occur on short string is enough to storage
     */
    constexpr void shrink_to_fit() noexcept
    {
        if (is_long_() && size_() <= short_str_max_)
        {
            // copy to local
            auto const ls = long_str_();
            short_str_(size_());
            ::std::copy(ls.begin(), ls.end(), short_str_().data());
            dealloc_(ls);
        }
    }

    friend struct construct_guard_;

    struct construct_guard_
    {
        basic_string *self;

        constexpr void release() noexcept
        {
            self = nullptr;
        }

        constexpr ~construct_guard_()
        {
            if (self)
                self->dealloc_(self->is_long_());
        }
    };

    // ************************* begin element access *************************

  private:
    /**
     * @return a pointer to the first element
     */
    constexpr CharT const *begin_() const noexcept
    {
        return is_short_() ? short_str_().data() : long_str_().begin();
    }

    /**
     * @return a pointer to the first element
     */
    constexpr CharT *begin_() noexcept
    {
        return is_short_() ? short_str_().data() : long_str_().begin();
    }

    /**
     * @return a pointer to the next position of the last element
     */
    constexpr CharT const *end_() const noexcept
    {
        return is_short_() ? short_str_().data() + size_flag_ : long_str_().end();
    }

    /**
     * @return a pointer to the next position of the last element
     */
    constexpr CharT *end_() noexcept
    {
        return is_short_() ? short_str_().data() + size_flag_ : long_str_().end();
    }

  public:
    constexpr CharT const *data() const noexcept
    {
        return begin_();
    }

    constexpr CharT *data() noexcept
    {
        return begin_();
    }

    constexpr CharT const *c_str() const noexcept
    {
        return begin_();
    }

    constexpr const_reference at(size_type pos) const
    {
        if (pos < size_())
            return *(begin_() + pos);
        throw_out_of_range_();
    }

    constexpr reference at(size_type pos)
    {
        if (pos < size_())
            return *(begin_() + pos);
        throw_out_of_range_();
    }

    constexpr const_reference operator[](size_type pos) const noexcept
    {
        assert(pos <= size_());

        return *(begin_() + pos);
    }

    constexpr reference operator[](size_type pos) noexcept
    {
        assert(pos <= size_());

        return *(begin_() + pos);
    }

    constexpr const_reference front() const noexcept
    {
        assert(!empty());

        return *begin_();
    }

    constexpr reference front()
    {
        assert(!empty());

        return *begin_();
    }

    constexpr const_reference back() const noexcept
    {
        assert(!empty());

        return *(end_() - 1uz);
    }

    constexpr reference back()
    {
        assert(!empty());

        return *(end_() - 1uz);
    }

    constexpr operator ::std::basic_string_view<value_type, traits_type>() const noexcept
    {
        return view_();
    }

    // ************************ begin iterator function ************************

  public:
    using iterator = string_detail::string_iterator<value_type, Alloc>;
    using const_iterator = string_detail::string_iterator<value_type const, Alloc>;
    using reverse_iterator = ::std::reverse_iterator<iterator>;
    using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

    friend iterator;
    friend const_iterator;

    constexpr iterator begin() noexcept
    {
#if !defined(NDEBUG)
        return {begin_(), begin_(), end_()};
#else
        return {begin_()};
#endif
    }

    constexpr iterator end() noexcept
    {
#if !defined(NDEBUG)
        return {end_(), begin_(), end_()};
#else
        return {end_()};
#endif
    }

    constexpr const_iterator begin() const noexcept
    {
#if !defined(NDEBUG)
        return {begin_(), begin_(), end_()};
#else
        return {begin_()};
#endif
    }

    constexpr const_iterator end() const noexcept
    {
#if !defined(NDEBUG)
        return {end_(), begin_(), end_()};
#else
        return {end_()};
#endif
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

    constexpr auto rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    constexpr auto rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    constexpr auto rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr auto crbegin() const noexcept
    {
        return const_reverse_iterator{cend()};
    }

    constexpr auto crend() const noexcept
    {
        return const_reverse_iterator{cbegin()};
    }

    // ************************ begin memory management ************************

  private:
    /**
     * @brief allocates memory and automatically adds 1 to store null terminator
     * @param n, expected number of characters
     */
    constexpr ls_type_ allocate_(size_type cap, size_type size)
    {
#if defined(__cpp_lib_allocate_at_least) && (__cpp_lib_allocate_at_least >= 202302L)
        auto const [ptr, count] = atraits_t_::allocate_at_least(
            allocator_, static_cast<atraits_t_::size_type>(cap + 1uz /* null terminator */));

        if consteval
        {
            for (auto i = 0uz; i != count; ++i)
                ::std::construct_at(::std::addressof(ptr[i]));
        }

        return {ptr, ::std::to_address(ptr) + size, ::std::to_address(ptr) + count - 1uz /* null terminator */};
#else
        auto const ptr =
            atraits_t_::allocate(allocator_, static_cast<atraits_t_::size_type>(cap + 1uz /* null terminator */));

        if consteval
        {
            for (auto i = 0uz; i != cap + 1uz; ++i)
                ::std::construct_at(::std::addressof(ptr[i]));
        }

        return {ptr, ::std::to_address(ptr) + size, ::std::to_address(ptr) + cap};
#endif
    }

    /**
     * @brief dealloc the memory of long string
     * @param ls, allocated long string
     */
    constexpr void dealloc_(ls_type_ const &ls) noexcept
    {
        atraits_t_::deallocate(
            allocator_, ls.begin_,
            static_cast<atraits_t_::size_type>(ls.last_ - ::std::to_address(ls.begin_) + 1uz /* null terminator */));
    }

    constexpr void dealloc_(bool is_long) noexcept
    {
        // avoid access the inactive member of union
        if (is_long)
            dealloc_(long_str_());
    }

    /**
     * @brief caculate the length of c style string
     * @param begin begin of characters
     * @return length
     */
    constexpr static size_type c_string_length_common_(CharT const *begin) noexcept
    {
        auto end = begin;

        for (; !traits_type::eq(*end, CharT{}); ++end)
            ;

        return end - begin;
    }
    constexpr static size_type c_string_length_(CharT const *begin) noexcept
    {
        if !consteval
        {
            if constexpr (::std::is_same_v<char, CharT> || ::std::is_same_v<char8_t, CharT>)
                return ::std::strlen(reinterpret_cast<char const *>(begin));
            else if constexpr (::std::is_same_v<wchar_t, CharT>)
                return ::std::wcslen(begin);
            else
                return c_string_length_common_(begin);
        }
        else
        {
            return c_string_length_common_(begin);
        }
    }

    /**
     * @brief assign characters to *this
     * @brief this function can be called with any legal state
     * @brief strong exception safety guarantee
     */
    constexpr void assign_(CharT const *first, CharT const *last, bool is_long, size_type new_size)
    {
        assert(is_long == is_long_());
        auto const ls = allocate_(new_size, new_size);
        ::std::copy(first, last, ls.begin());
        dealloc_(is_long);
        long_str_(ls);
    }

    constexpr void assign_(CharT const *first, CharT const *last)
    {
        auto const new_size = static_cast<size_type>(last - first);
        auto const is_long = is_long_();

        if !consteval
        {
            if (capacity() >= new_size)
            {
                auto const begin = begin_();
                ::std::less_equal less_eq;
                if (!(less_eq(first, begin) && less_eq(begin, first)))
                    ::std::copy(first, last, begin);

                resize_shrink_(is_long, new_size);
            }
            else
            {
                assign_(first, last, is_long, new_size);
            }
        }
        else
        {
            assign_(first, last, is_long, new_size);
        }
    }

    /**
     * @brief insert characters to *this
     * @brief this function can be called with any legal state
     * @brief strong exception safety guarantee
     */
    constexpr void insert_(size_type index, CharT const *first, CharT const *last, CharT *begin, CharT *end,
                           bool is_long, size_type length, size_type new_size)
    {

        auto const ls = allocate_(new_size, new_size);
        ::std::copy(begin, begin + index, ls.begin());
        ::std::copy(begin + index, end, ls.begin() + index + length);
        ::std::copy(first, last, ls.begin() + index);
        dealloc_(is_long);
        long_str_(ls);
    }

    constexpr void insert_(size_type index, CharT const *first, CharT const *last)
    {
        if (index > size_())
            throw_out_of_range_();

        auto const length = static_cast<size_type>(last - first);
        auto const new_size = size_() + length;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if !consteval
        {
            if (capacity() >= new_size && !overlap_(first, last, begin, end))
            {
                ::std::copy_backward(begin + index, end, begin + new_size);
                ::std::copy(first, last, begin + index);
                resize_shrink_(is_long, new_size);
            }
            else
            {
                insert_(index, first, last, begin, end, is_long, length, new_size);
            }
        }
        else
        {
            insert_(index, first, last, begin, end, is_long, length, new_size);
        }
    }

    constexpr void replace_(size_type pos, size_type count, CharT const *first, CharT const *last, CharT *begin,
                            CharT *end, bool is_long, size_type length, size_type new_size)
    {
        auto const ls = allocate_(new_size, new_size);
        ::std::copy(begin, begin + pos, ls.begin());
        ::std::copy(first, last, ls.begin() + pos);
        ::std::copy(begin + pos + count, end, ls.begin() + pos + length);
        dealloc_(is_long);
        long_str_(ls);
    }

    constexpr void replace_(size_type pos, size_type count, CharT const *first, CharT const *last)
    {
        if (pos > size_())
            throw_out_of_range_();

        count = ::std::min(size_() - pos, count);
        auto const length = static_cast<size_type>(last - first);
        auto const new_size = size_() - count + length;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if !consteval
        {
            if (capacity() >= new_size && !overlap_(first, last, begin, end))
            {
                if (count < length)
                    ::std::copy(begin + pos + count, end, begin + pos + length);
                else
                    ::std::copy_backward(begin + pos + count, end, begin + new_size);

                ::std::copy(first, last, begin + pos);
                resize_shrink_(is_long, new_size);
            }
            else
            {
                replace_(pos, count, first, last, begin, end, is_long, length, new_size);
            }
        }
        else
        {
            replace_(pos, count, first, last, begin, end, is_long, length, new_size);
        }
    }

  public:
    constexpr ~basic_string()
    {
        dealloc_(is_long_());
    }

    /**
     * @brief reserve memory
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param new_cap new capacity
     */
    constexpr void reserve()
    {
        reserve(size_());
    }
    constexpr void reserve(size_type new_cap)
    {
        if (new_cap <= capacity())
            return;
        else if (new_cap > max_size())
            throw ::std::length_error("string too long");
        else
        {
            auto const size = size_();
            auto const begin = begin_();
            auto const end = end_();
            auto const is_long = is_long_();
            auto const ls = allocate_(new_cap, size);
            ::std::copy(begin, end, ls.begin());
            dealloc_(is_long);
            long_str_(ls);
        }
    }

    /**
     * @brief resize string length
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param count new size
     * @param ch character to fill
     */
    constexpr void resize(size_type count, value_type ch)
    {
        if (auto const size = size_(); count < size)
        {
            resize_shrink_(is_long_(), count);
        }
        else if (count > size)
        {
            reserve(count);
            ::std::fill(end_(), end_() + (count - size), ch);
            resize_shrink_(is_long_(), count);
        }
    }

    /**
     * @brief resize string length
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param count new size
     */
    constexpr void resize(size_type count)
    {
        resize(count, CharT{});
    }

    /**
     * @brief never shrink_to_fit
     */
    constexpr void clear() noexcept
    {
        resize_shrink_(is_long_(), 0uz);
    }

    /**
     * @brief use size * 1.5 for growth
     * @param ch character to fill
     */
    constexpr void push_back(CharT ch)
    {
        auto const size = size_();

        if (capacity() == size)
        {
            auto const msize = max_size();
            if (size >= msize)
                throw_length_error_();
            // NB: During constant evaluation, size == cap == 0 may occur
            reserve(::std::min(size + size_type(1) + size / size_type(2), msize));
        }

        *end_() = ch;
        resize_shrink_(is_long_(), size + size_type(1));
    }

    // ****************************** begin swap ******************************

  private:
    constexpr void swap_without_ator(basic_string &other) noexcept
    {
        auto &self = *this;
        using std::swap;
        swap(self.stor_, other.stor_);
        swap(self.size_flag_, other.size_flag_);
    }

  public:
    constexpr void swap(basic_string &other) noexcept
    {
        using std::swap;
        if constexpr (atraits_t_::propagate_on_container_swap::value)
            swap(other.allocator_, other.allocator_);
        else
            assert(other.allocator_ == allocator_);

        other.swap_without_ator(*this);
    }

    friend constexpr void swap(basic_string &lhs, basic_string &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // *************************** begin constructor ***************************

    constexpr void construct_(::std::size_t n)
    {
        if (short_str_max_ >= n)
            resize_shrink_(false, n);
        else if (n > max_size())
            throw_length_error_();
        else
            long_str_(allocate_(n, n));
    }

  public:
    constexpr basic_string() noexcept(noexcept(allocator_type())) : allocator_(allocator_type())
    {
    }

    constexpr explicit basic_string(allocator_type const &a) noexcept : allocator_(a)
    {
    }

    template <string_detail::mini_alloc IAlloc = Alloc>
    constexpr basic_string(size_type count, value_type ch, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        construct_(count);
        ::std::fill(begin_(), end_(), ch);
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string(U first, V last, const allocator_type &alloc = allocator_type()) : allocator_(alloc)
    {
        construct_guard_ g{this};

        if constexpr (::std::random_access_iterator<U>)
        {
            construct_(last - first);
            ::std::copy(first, last, begin_());
        }
        else
        {
            for (; first != last; ++first)
                push_back(*first);
        }

        g.release();
    }

#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
    // tagged constructors to construct from container compatible range
    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string(::std::from_range_t, R &&rg, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        auto first = ::std::ranges::begin(rg);
        auto last = ::std::ranges::end(rg);

        construct_guard_ g{this};

        if constexpr (::std::ranges::sized_range<R>)
        {
            construct_(::std::ranges::size(rg));

            if constexpr (::std::ranges::forward_range<R>)
            {
                ::std::ranges::copy(::std::ranges::begin(rg), ::std::ranges::end(rg), begin_());
            }
            else
            {
                for (auto begin = begin_(); first != last; ++first, ++begin)
                    *begin = *first;
            }
        }
        else
        {
            for (; first != last; ++first)
                push_back(*first);
        }

        g.release();
    }
#endif

    constexpr basic_string(CharT const *s, size_type count, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        construct_(count);
        ::std::copy(s, s + count, begin_());
    }

    template <string_detail::mini_alloc IAlloc = Alloc>
    constexpr basic_string(CharT const *s, allocator_type const &a = allocator_type())
        : basic_string(s, c_string_length_(s), a)
    {
    }

    constexpr basic_string(::std::nullptr_t) = delete;

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    explicit constexpr basic_string(StringViewLike const &t, allocator_type const &a = allocator_type())
        : basic_string(::std::basic_string_view<value_type, traits_type>{t}.data(),
                       ::std::basic_string_view<value_type, traits_type>{t}.size(), a)
    {
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>>
    constexpr basic_string(StringViewLike const &t, size_type pos, size_type n,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos > sv.size())
            throw_out_of_range_();

        n = ::std::min(static_cast<size_type>(sv.size() - pos), n);
        construct_(n);
        ::std::copy(sv.data() + pos, sv.data() + pos + n, begin_());
    }

    constexpr basic_string(basic_string const &other)
        : allocator_(atraits_t_::select_on_container_copy_construction(other.allocator_))
    {
        construct_(other.size_());
        ::std::copy(other.begin_(), other.end_(), begin_());
    }

    constexpr basic_string(basic_string &&other) noexcept : allocator_(other.allocator_)
    {
        other.swap_without_ator(*this);
    }

    constexpr basic_string(const basic_string &other, allocator_type const &a) : allocator_(a)
    {
        construct_(other.size_());
        ::std::copy(other.begin_(), other.end_(), begin_());
    }

    constexpr basic_string(basic_string const &other, size_type pos, allocator_type const &a = allocator_type())
        : basic_string(other, pos, npos /* end */, a)
    {
    }

    constexpr basic_string(basic_string &&other, allocator_type const &a) : allocator_(a)
    {
        assert(allocator_ == a);

        if (other.allocator_ == allocator_)
        {
            other.swap_without_ator(*this);
        }
        else
        {
            construct_(other.size_());
            ::std::copy(other.begin_(), other.end_(), begin_());
        }
    }

    constexpr basic_string(basic_string const &other, size_type pos, size_type count,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        if (pos > other.size_())
            throw_out_of_range_();

        count = ::std::min(other.size_() - pos, count);
        construct_(count);
        ::std::copy(other.begin_() + pos, other.begin_() + pos + count, begin_());
    }

    constexpr basic_string(basic_string &&other, size_type pos, size_type count,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        assert(allocator_ == a);

        if (pos > other.size_())
            throw_out_of_range_();

        count = ::std::min(other.size_() - pos, count);

        if (other.allocator_ == allocator_)
        {
            if (pos != 0uz)
                ::std::copy(other.begin_() + pos, other.begin_() + pos + count, other.begin_());

            other.resize_shrink_(other.is_long_(), count);
            stor_ = other.stor_;
            size_flag_ = other.size_flag_;
            other.stor_ = storage_type_{};
            other.short_str_(0uz);
        }
        else
        {
            construct_(count);
            ::std::copy(other.begin_() + pos, other.begin_() + pos + count, begin_());
        }
    }

    constexpr basic_string(::std::initializer_list<value_type> ilist, allocator_type const &a = allocator_type())
        : basic_string(ilist.begin(), ilist.size(), a)
    {
    }

    // ***************************** begin assign *****************************

  private:
    constexpr void copy_ator_and_move_(basic_string &str) noexcept
    {
        dealloc_(is_long_());
        allocator_ = str.allocator_;
        stor_ = str.stor_;
        size_flag_ = str.size_flag_;
        str.stor_ = {};
        str.size_flag_ = 0u;
    }

  public:
    constexpr basic_string &assign(const basic_string &str)
    {
        if (::std::addressof(str) == this)
            return *this;

        if constexpr (atraits_t_::propagate_on_container_copy_assignment::value)
        {
            if (allocator_ != str.allocator_)
            {
                basic_string temp{str, str.allocator_};
                copy_ator_and_move_(temp);

                return *this;
            }
        }

        assign_(str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &assign(basic_string &&str) noexcept(
        atraits_t_::propagate_on_container_move_assignment::value || atraits_t_::is_always_equal::value)
    {
        if constexpr (atraits_t_::propagate_on_container_move_assignment::value)
        {
            copy_ator_and_move_(str);
        }
        else
        {
            if constexpr (!atraits_t_::is_always_equal::value)
            {
                if (allocator_ == str.allocator_)
                {
                    str.swap_without_ator(*this);

                    return *this;
                }
            }

            assign_(str.begin_(), str.end_());
        }

        return *this;
    }

    constexpr basic_string &assign(size_type count, CharT ch)
    {
        if (count > max_size())
            throw_length_error_();
        else if (capacity() < count)
        {
            auto const size = size_();
            auto const is_long = is_long_();
            auto const ls = allocate_(count, size);
            dealloc_(is_long);
            long_str_(ls);
        }

        auto const is_long = is_long_();
        ::std::fill(begin_(), begin_() + count, ch);
        resize_shrink_(is_long, count);

        return *this;
    }

    constexpr basic_string &assign(CharT const *s, size_type count)
    {
        assign_(s, s + count);

        return *this;
    }

    constexpr basic_string &assign(CharT const *s)
    {
        assign_(s, s + c_string_length_(s));

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &assign(StringViewLike const &t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        assign_(sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &assign(StringViewLike const &t, size_type pos, size_type count = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos > sv.size())
            throw_out_of_range_();

        count = ::std::min(static_cast<size_type>(sv.size() - pos), count);
        assign_(sv.data() + pos, sv.data() + pos + count);

        return *this;
    }

    constexpr basic_string &assign(const basic_string &str, size_type pos, size_type count = npos)
    {
        if (pos > str.size_())
            throw_out_of_range_();

        count = ::std::min(str.size_() - pos, count);
        assign_(str.begin_() + pos, str.begin_() + pos + count);

        return *this;
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string &assign(U first, V last)
    {
        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            assign_(::std::to_address(first), ::std::to_address(first) + ::std::distance(first, last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            assign_(temp.begin_(), temp.end_());
        }

        return *this;
    }

    constexpr basic_string &assign(::std::initializer_list<CharT> ilist)
    {
        assign_(ilist.begin(), ilist.end());

        return *this;
    }

    // **************************** begin operator= ****************************

    constexpr basic_string &operator=(const basic_string &str)
    {
        return assign(str);
    }

    constexpr basic_string &operator=(basic_string &&other) noexcept(
        atraits_t_::propagate_on_container_move_assignment::value || atraits_t_::is_always_equal::value)
    {
        return assign(::std::move(other));
    }

    constexpr basic_string &operator=(CharT const *s)
    {
        return assign(s);
    }

    constexpr basic_string &operator=(CharT ch)
    {
        resize_shrink_(is_long_(), 1uz);
        *begin_() = ch;

        return *this;
    }

    constexpr basic_string &operator=(::std::initializer_list<CharT> ilist)
    {
        return assign(ilist);
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &operator=(StringViewLike const &t)
    {
        return assign(t);
    }

    constexpr basic_string &operator=(::std::nullptr_t) = delete;

    // ***************************** begin compare *****************************

  private:
    static constexpr ::std::strong_ordering compare_(value_type const *first, size_type size, value_type const *first1,
                                                     size_type size1) noexcept
    {
        if !consteval
        {
            ::std::less_equal less_eq;

            if (less_eq(first, first1) && less_eq(first1, first))
                return size <=> size1;
        }

        for (auto const last = first + ::std::min(size, size1); first != last; ++first, ++first1)
        {
            if (*first > *first1)
                return ::std::strong_ordering::greater;
            else if (*first < *first1)
                return ::std::strong_ordering::less;
        }

        return size <=> size1;
    }

    static constexpr int unwrap_stong_ordering_(_STD strong_ordering od) noexcept
    {
        return od > 0 ? 1 : (od < 0 ? -1 : 0);
    }

  public:
    friend constexpr ::std::strong_ordering operator<=>(basic_string const &lhs, basic_string const &rhs) noexcept
    {
        return compare_(lhs.begin_(), lhs.size_(), rhs.begin_(), rhs.size_());
    }

    friend constexpr ::std::strong_ordering operator<=>(
        basic_string const &lhs, ::std::basic_string_view<value_type, traits_type> const &rhs) noexcept
    {
        return compare_(lhs.begin_(), lhs.size_(), rhs.data(), rhs.size());
    }

    friend constexpr ::std::strong_ordering operator<=>(basic_string const &lhs, CharT const *rhs) noexcept
    {
        return compare_(lhs.begin_(), lhs.size_(), rhs, basic_string::c_string_length_(rhs));
    }

    constexpr int compare(const basic_string &str) const noexcept
    {
        return order_to_int_(view_() <=> str.view_());
    }

    constexpr int compare(size_type pos1, size_type count1, const basic_string &str) const
    {
        return order_to_int_(view_().substr(pos1, count1) <=> str.view_());
    }

    constexpr int compare(size_type pos1, size_type count1, const basic_string &str, size_type pos2,
                          size_type count2 = npos) const
    {
        return order_to_int_(view_().substr(pos1, count1) <=> str.view_().substr(pos2, count2));
    }

    constexpr int compare(const CharT *s) const noexcept
    {
        return order_to_int_(view_() <=> ::std::basic_string_view<value_type, traits_type>(s));
    }

    constexpr int compare(size_type pos1, size_type count1, const CharT *s) const
    {
        return order_to_int_(view_().substr(pos1, count1) <=> ::std::basic_string_view<value_type, traits_type>(s));
    }

    constexpr int compare(size_type pos1, size_type count1, const CharT *s, size_type count2) const
    {
        return order_to_int_(view_().substr(pos1, count1) <=>
                                ::std::basic_string_view<value_type, traits_type>(s, count2));
    }

    template <class StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr int compare(const StringViewLike &t) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return order_to_int_(view_() <=> tsv);
    }

    template <class StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr int compare(size_type pos1, size_type count1, const StringViewLike &t) const
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return order_to_int_(view_().substr(pos1, count1) <=> tsv);
    }

    template <class StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr int compare(size_type pos1, size_type count1, const StringViewLike &t, size_type pos2,
                          size_type count2 = npos) const
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return order_to_int_(view_().substr(pos1, count1) <=> tsv.substr(pos2, count2));
    }

  private:
    constexpr bool static equal_(CharT const *begin, CharT const *end, CharT const *first, CharT const *last) noexcept
    {
        if (last - first != end - begin)
            return false;

        if consteval
        {
            for (; begin != end; ++begin, ++first)
            {
                if (!traits_type::eq(*first, *begin))
                    return false;
            }

            return true;
        }
        else
        {
            return begin == end || ::std::memcmp(begin, first, (end - begin) * sizeof(CharT)) == 0;
        }
    }

  public:
    friend constexpr bool operator==(basic_string const &lhs, basic_string const &rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs.begin_(), rhs.end_());
    }

    // Used to resolve ambiguation with operator== provided by ::std::basic_string_view.
    friend constexpr bool operator==(basic_string const &lhs,
                                     ::std::basic_string_view<value_type, traits_type> rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs.data(), rhs.data() + rhs.size());
    }

    friend constexpr bool operator==(basic_string const &lhs, CharT const *rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs, rhs + c_string_length_(rhs));
    }

    // ***************************** begin append *****************************

  private:
    constexpr void append_(CharT const *first, CharT const *last, CharT *begin, CharT *end, bool is_long,
                           size_type size, size_type new_size)
    {
        auto const ls = allocate_(new_size, new_size);
        ::std::copy(begin, end, ls.begin());
        ::std::copy(first, last, ls.begin() + size);
        dealloc_(is_long);
        long_str_(ls);
    }

    constexpr void append_(CharT const *first, CharT const *last)
    {
        auto const length = static_cast<size_type>(last - first);
        auto const size = size_();
        auto const begin = begin_();
        auto const end = end_();
        auto const new_size = size + length;
        auto const is_long = is_long_();

        if !consteval
        {
            if (capacity() >= new_size && !overlap_(first, last, begin, end))
            {
                ::std::copy(first, last, end);
                resize_shrink_(is_long, new_size);
            }
            else
            {
                append_(first, last, begin, end, is_long, size, new_size);
            }
        }
        else
        {
            append_(first, last, begin, end, is_long, size, new_size);
        }
    }

  public:
    constexpr basic_string &append(size_type count, CharT ch)
    {
        auto const size = size_();
        reserve(size + count);
        ::std::fill(end_(), end_() + count, ch);
        resize_shrink_(is_long_(), size + count);

        return *this;
    }

    constexpr basic_string &append(CharT const *s, size_type count)
    {
        append_(s, s + count);

        return *this;
    }

    constexpr basic_string &append(CharT const *s)
    {
        append_(s, s + c_string_length_(s));

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &append(StringViewLike const &t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        append_(sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &append(StringViewLike const &t, size_type pos, size_type count = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos > sv.size())
            throw_out_of_range_();

        count = ::std::min(static_cast<size_type>(sv.size() - pos), count);
        append_(sv.data() + pos, sv.data() + pos + count);

        return *this;
    }

    constexpr basic_string &append(const basic_string &str)
    {
        append_(str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &append(const basic_string &str, size_type pos, size_type count = npos)
    {
        if (pos > str.size_())
            throw_out_of_range_();

        count = ::std::min(str.size_() - pos, count);
        append_(str.begin_() + pos, str.begin_() + pos + count);

        return *this;
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string &append(U first, V last)
    {
        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            append_(::std::to_address(first), ::std::to_address(first) + ::std::distance(first, last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            append_(temp.begin_(), temp.end_());
        }

        return *this;
    }

    constexpr basic_string &append(::std::initializer_list<CharT> ilist)
    {
        append_(ilist.begin(), ilist.end());

        return *this;
    }

    // *************************** begin operator+= ***************************

    constexpr basic_string &operator+=(const basic_string &str)
    {
        return append(str);
    }

    constexpr basic_string &operator+=(CharT ch)
    {
        push_back(ch);

        return *this;
    }

    constexpr basic_string &operator+=(CharT const *s)
    {
        return append(s);
    }

    constexpr basic_string &operator+=(::std::initializer_list<CharT> ilist)
    {
        return append(ilist);
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &operator+=(StringViewLike const &t)
    {
        return append(t);
    }

    // ***************************** begin search *****************************

  public:
    constexpr bool starts_with(::std::basic_string_view<value_type, traits_type> sv) const noexcept
    {
        return sv.size() <= size_() && equal_(sv.data(), sv.data() + sv.size(), begin_(), begin_() + sv.size());
    }

    constexpr bool starts_with(CharT ch) const noexcept
    {
        return *begin_() == ch;
    }

    constexpr bool starts_with(CharT const *s) const
    {
        auto const length = c_string_length_(s);

        return length <= size_() && equal_(s, s + length, begin_(), begin_() + length);
    }

    constexpr bool ends_with(::std::basic_string_view<value_type, traits_type> sv) const noexcept
    {
        return sv.size() <= size_() && equal_(sv.data(), sv.data() + sv.size(), end_() - sv.size(), end_());
    }

    constexpr bool ends_with(CharT ch) const noexcept
    {
        return !empty() && *(end_() - 1) == ch;
    }

    constexpr bool ends_with(CharT const *s) const
    {
        auto const length = c_string_length_(s);

        return length <= size_() && equal_(s, s + length, end_() - length, end_());
    }

#if defined(__cpp_lib_string_contains) && (__cpp_lib_string_contains >= 202011L)
    constexpr bool contains(::std::basic_string_view<value_type, traits_type> sv) const noexcept
    {
        return view_().contains(sv);
    }

    constexpr bool contains(CharT ch) const noexcept
    {
        for (auto begin = begin_(), end = end_(); begin != end; ++begin)
        {
            if (*begin == ch)
                return true;
        }

        return false;
    }

    constexpr bool contains(CharT const *s) const noexcept
    {
        return view_().contains(s);
    }
#endif

    // ***************************** begin substr *****************************

    constexpr basic_string substr(size_type pos = 0uz, size_type count = npos) const &
    {
        return basic_string{*this, pos, count};
    }

    constexpr basic_string substr(size_type pos = 0uz, size_type count = npos) &&
    {
        return basic_string{::std::move(*this), pos, count};
    }

    constexpr ::std::basic_string_view<value_type, traits_type> subview(size_type pos = 0uz, size_type count = npos) const
    {
        if (pos > size_())
            throw ::std::out_of_range("string_view::substr");

        return view_().substr(pos, count);
    }


    // ***************************** begin insert *****************************

    constexpr basic_string &insert(size_type index, size_type count, value_type ch)
    {
        if (index > size_())
            throw_out_of_range_();

        auto const new_size = size_() + count;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size)
        {
            ::std::copy_backward(begin + index, end, end + count);
            ::std::fill(begin + index, begin + index + count, ch);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::copy(begin, begin + index, ls.begin());
            ::std::copy(begin + index, end, ls.begin() + index + count);
            ::std::fill(ls.begin() + index, ls.begin() + index + count, ch);
            dealloc_(is_long);
            long_str_(ls);
        }

        return *this;
    }

    constexpr basic_string &insert(size_type index, CharT const *s)
    {
        insert_(index, s, s + c_string_length_(s));

        return *this;
    }

    constexpr basic_string &insert(size_type index, CharT const *s, size_type count)
    {
        insert_(index, s, s + count);

        return *this;
    }

    constexpr basic_string &insert(size_type index, const basic_string &str)
    {
        insert_(index, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &insert(size_type index, const basic_string &str, size_type s_index, size_type count = npos)
    {
        if (s_index > str.size_())
            throw_out_of_range_();

        count = ::std::min(str.size_() - s_index, count);
        insert_(index, str.begin_() + s_index, str.begin_() + s_index + count);

        return *this;
    }

    constexpr iterator insert(const_iterator pos, CharT ch)
    {
        assert(pos.current_ >= begin_() && pos.current_ <= end_());
        auto const size = size_();
        auto const begin = begin_();
        auto const index = begin + (pos.current_ - begin);
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= size + 1uz)
        {
            ::std::copy_backward(index, end, end + 1uz);
            *index = ch;
            resize_shrink_(is_long, size + 1);

#if !defined(NDEBUG)
            return {index, begin, end};
#else
            return {index};
#endif
        }
        else
        {
            auto const ls = allocate_(size * 2uz - size / 2uz, size + 1uz);
            ::std::copy(begin, index, ls.begin());
            auto const new_index = ls.begin() + (index - begin);
            *new_index = ch;
            ::std::copy(index, end, new_index + 1uz);
            dealloc_(is_long);
            long_str_(ls);

#if !defined(NDEBUG)
            return {new_index, ls.begin(), ls.end()};
#else
            return {new_index};
#endif
        }
    }

    constexpr iterator insert(const_iterator pos, size_type count, CharT ch)
    {
        auto const index = pos.current_ - begin_();
        insert(index, count, ch);

        return {begin() + index};
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr iterator insert(const_iterator pos, U first, V last)
    {
        assert(pos.current_ >= begin_() && pos.current_ <= end_());
        auto const index = pos.current_ - begin_();

        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            insert_(index, ::std::to_address(first), ::std::to_address(first) + ::std::distance(first, last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            insert_(index, temp.begin_(), temp.end_());
        }

        return {begin() + index};
    }

    constexpr iterator insert(const_iterator pos, ::std::initializer_list<CharT> ilist)
    {
        auto const index = pos.current_ - begin_();
        insert_(index, ilist.begin(), ilist.begin() + ilist.size());

        return {begin() + index};
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &insert(size_type pos, StringViewLike const &t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        insert_(pos, sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &insert(size_type pos, StringViewLike const &t, size_type t_index, size_type count = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (t_index > sv.size())
            throw_out_of_range_();

        count = ::std::min(static_cast<size_type>(sv.size() - t_index), count);
        insert_(pos, sv.data() + t_index, sv.data() + t_index + count);

        return *this;
    }

    // ****************************** begin erase ******************************

  private:
    constexpr void erase_(CharT *first, CharT const *last) noexcept
    {
        assert(first >= begin_() && last <= end_());
        auto const is_long = is_long_();
        auto const size = size_();
        ::std::copy(last, static_cast<CharT const *>(end_()), first);
        resize_shrink_(is_long, size - (last - first));
    }

  public:
    constexpr basic_string &erase(size_type index = 0uz, size_type count = npos)
    {
        if (index > size_())
            throw_out_of_range_();

        count = ::std::min(size_() - index, count);
        erase_(begin_() + index, begin_() + index + count);

        return *this;
    }

    constexpr iterator erase(const_iterator position) noexcept
    {
        auto const start = begin_() + (position.current_ - begin_());
        assert(start >= begin_() && start < end_());
        auto const is_long = is_long_();
        auto const size = size_();
        ::std::copy(start + 1uz, end_(), start);
        resize_shrink_(is_long, size - 1uz);

#if !defined(NDEBUG)
        return {start, begin_(), end_()};
#else
        return {start};
#endif
    }

    constexpr iterator erase(const_iterator first, const_iterator last) noexcept
    {
        auto const start = begin_() + (first.current_ - begin_());
        erase_(start, last.current_);

#if !defined(NDEBUG)
        return {start, begin_(), end_()};
#else
        return {start};
#endif
    }

    // **************************** begin pop_back ****************************

    constexpr void pop_back() noexcept
    {
        assert(!empty());
        resize_shrink_(is_long_(), size_() - 1uz);
    }

    // ***************************** begin replace *****************************

    constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str)
    {
        replace_(pos, count, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, const basic_string &str)
    {
        auto const start = first.current_;
        assert(start >= begin_() && start <= end_() && first <= last);
        replace_(start - begin_(), last - first, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str, size_type pos2,
                                    size_type count2 = npos)
    {
        if (pos2 > str.size_())
            throw_out_of_range_();

        count2 = ::std::min(count2, str.size_() - pos2);
        replace_(pos, count, str.begin_() + pos2, str.begin_() + pos2 + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, CharT const *cstr, size_type count2)
    {
        replace_(pos, count, cstr, cstr + count2);

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, CharT const *cstr, size_type count2)
    {
        assert(first >= begin() && last <= end() && first <= last);
        replace_(first.current_ - begin_(), last - first, cstr, cstr + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, CharT const *cstr)
    {
        replace_(pos, count, cstr, cstr + c_string_length_(cstr));

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, CharT const *cstr)
    {
        assert(first >= begin() && last <= end() && first <= last);
        replace_(first.current_ - begin_(), last - first, cstr, cstr + c_string_length_(cstr));

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, ::std::initializer_list<CharT> ilist)
    {
        assert(first <= last);
        replace_(first.current_ - begin_(), last - first, ilist.begin(), ilist.begin() + ilist.size());

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &replace(size_type pos, size_type count, const StringViewLike &t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        replace_(pos, count, sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &replace(const_iterator first, const_iterator last, const StringViewLike &t)
    {
        assert(first >= begin() && last <= end() && first <= last);
        ::std::basic_string_view<value_type, traits_type> sv = t;
        replace_(first.current_ - begin_(), static_cast<size_type>(last - first), sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr basic_string &replace(size_type pos, size_type count, const StringViewLike &t, size_type pos2,
                                    size_type count2 = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos2 > sv.size())
            throw_out_of_range_();

        count2 = ::std::min(static_cast<size_type>(sv.size() - pos2), count2);
        replace_(pos, count, sv.data() + pos2, sv.data() + pos2 + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, size_type count2, CharT ch)
    {
        if (pos > size_())
            throw_out_of_range_();

        count = ::std::min(size_() - pos, count);
        auto const size = size_();
        auto const new_size = size - count + count2;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size)
        {
            if (count > count2)
                ::std::copy(begin + pos + count, end, begin + pos + count2);
            else if (count < count2)
                ::std::copy_backward(begin + pos + count, end, begin + new_size);

            ::std::fill(begin + pos, begin + pos + count2, ch);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::copy(begin, begin + pos, ls.begin());
            ::std::copy(begin + pos + count, end, ls.begin() + pos + count2);
            ::std::fill(ls.begin() + pos, ls.begin() + pos + count2, ch);
            dealloc_(is_long);
            long_str_(ls);
        }

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, size_type count2, CharT ch)
    {
        assert(first <= last);
        replace(first.current_ - begin_(), last - first, count2, ch);

        return *this;
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string &replace(const_iterator first, const_iterator last, U first2, V last2)
    {
        assert(first <= last);
        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            replace_(first.current_ - begin_(), last - first, ::std::to_address(first2),
                     ::std::to_address(first2) + ::std::distance(first2, last2));
        }
        else
        {
            basic_string temp{first2, last2, allocator_};
            replace_(first.current_ - begin_(), last - first, temp.begin_(), temp.end_());
        }

        return *this;
    }

    // ************* begin assign_range/insert_range/append_range *************

#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string &assign_range(R &&rg)
    {
        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first = ::std::ranges::begin(rg);
            auto const last = ::std::ranges::end(rg);
            assign_(::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            assign(basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return *this;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr iterator insert_range(const_iterator pos, R &&rg)
    {
        auto const index = pos - begin();

        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first = ::std::ranges::begin(rg);
            auto const last = ::std::ranges::end(rg);
            insert_(index, ::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            insert(index, basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return begin() + index;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string &append_range(R &&rg)
    {
        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first = ::std::ranges::begin(rg);
            auto const last = ::std::ranges::end(rg);
            append_(::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            append(basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return *this;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string &replace_with_range(const_iterator first, const_iterator last, R &&rg)
    {
        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first2 = ::std::ranges::begin(rg);
            auto const last2 = ::std::ranges::end(rg);
            replace_(first.current_ - begin_(), last - first, ::std::to_address(first2),
                     ::std::to_address(first2) + ::std::ranges::distance(first2, last2));
        }
        else
        {
            replace(first, last, basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return *this;
    }
#endif

    template <class Operation>
    constexpr void resize_and_overwrite(size_type count, Operation op)
    {
        reserve(count);
        resize_shrink_(is_long_(), ::std::move(op)(begin_(), size_type(count)));
    }

    constexpr allocator_type get_allocator() const noexcept
    {
        return allocator_;
    }

    constexpr size_type copy(CharT *dest, size_type count, size_type pos = 0uz) const
    {
        if (pos > size_())
            throw_out_of_range_();

        count = ::std::min(count, size_() - pos);

        ::std::copy(begin_() + pos, begin_() + pos + count, dest);

        return count;
    }

    constexpr size_type find(const basic_string &str, size_type pos = 0uz) const
    {
        return view_().find(str.view_(), pos);
    }

    constexpr size_type find(const CharT *s, size_type pos, size_type count) const
    {
        return view_().find(s, pos, count);
    }

    constexpr size_type find(const CharT *s, size_type pos = 0uz) const
    {
        return view_().find(s, pos);
    }

    constexpr size_type find(CharT ch, size_type pos = 0uz) const
    {
        return view_().find(ch, pos);
    }

    template <class StringViewLike>
        requires is_svlike_v_<StringViewLike>
    constexpr size_type find(const StringViewLike &t, size_type pos = 0uz) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return view_().find(tsv, pos);
    }

    constexpr size_type rfind(const basic_string &str, size_type pos = npos) const
    {
        return view_().rfind(str.view_(), pos);
    }

    constexpr size_type rfind(const CharT *s, size_type pos, size_type count) const
    {
        return view_().rfind(s, pos, count);
    }

    constexpr size_type rfind(const CharT *s, size_type pos = npos) const
    {
        return view_().rfind(s, pos);
    }

    constexpr size_type rfind(CharT ch, size_type pos = npos) const
    {
        return view_().rfind(ch, pos);
    }

    template <class StringViewLike>
    constexpr size_type rfind(const StringViewLike &t, size_type pos = npos) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return view_().rfind(tsv, pos);
    }

    constexpr size_type find_first_of(const basic_string &str, size_type pos = 0uz) const
    {
        return view_().find_first_of(str.view_(), pos);
    }

    constexpr size_type find_first_of(const CharT *s, size_type pos, size_type count) const
    {
        return view_().find_first_of(s, pos, count);
    }

    constexpr size_type find_first_of(const CharT *s, size_type pos = 0uz) const
    {
        return view_().find_first_of(s, pos);
    }

    constexpr size_type find_first_of(CharT ch, size_type pos = 0uz) const
    {
        return view_().find_first_of(ch, pos);
    }

    template <class StringViewLike>
    constexpr size_type find_first_of(const StringViewLike &t, size_type pos = 0uz) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return view_().find_first_of(tsv, pos);
    }

    constexpr size_type find_first_not_of(const basic_string &str, size_type pos = 0uz) const
    {
        return view_().find_first_not_of(str.view_(), pos);
    }

    constexpr size_type find_first_not_of(const CharT *s, size_type pos, size_type count) const
    {
        return view_().find_first_not_of(s, pos, count);
    }

    constexpr size_type find_first_not_of(const CharT *s, size_type pos = 0uz) const
    {
        return view_().find_first_not_of(s, pos);
    }

    constexpr size_type find_first_not_of(CharT ch, size_type pos = 0uz) const
    {
        return view_().find_first_not_of(ch, pos);
    }

    template <class StringViewLike>
    constexpr size_type find_first_not_of(const StringViewLike &t, size_type pos = 0uz) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return view_().find_first_not_of(tsv, pos);
    }

    constexpr size_type find_last_of(const basic_string &str, size_type pos = npos) const
    {
        return view_().find_last_of(str.view_(), pos);
    }

    constexpr size_type find_last_of(const CharT *s, size_type pos, size_type count) const
    {
        return view_().find_last_of(s, pos, count);
    }

    constexpr size_type find_last_of(const CharT *s, size_type pos = npos) const
    {
        return view_().find_last_of(s, pos);
    }

    constexpr size_type find_last_of(CharT ch, size_type pos = npos) const
    {
        return view_().find_last_of(ch, pos);
    }

    template <class StringViewLike>
    constexpr size_type find_last_of(const StringViewLike &t, size_type pos = npos) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return view_().find_last_of(tsv, pos);
    }

    constexpr size_type find_last_not_of(const basic_string &str, size_type pos = npos) const
    {
        return view_().find_last_not_of(str.view_(), pos);
    }

    constexpr size_type find_last_not_of(const CharT *s, size_type pos, size_type count) const
    {
        return view_().find_last_not_of(s, pos, count);
    }

    constexpr size_type find_last_not_of(const CharT *s, size_type pos = npos) const
    {
        return view_().find_last_not_of(s, pos);
    }

    constexpr size_type find_last_not_of(CharT ch, size_type pos = npos) const
    {
        return view_().find_last_not_of(ch, pos);
    }

    template <class StringViewLike>
    constexpr size_type find_last_not_of(const StringViewLike &t, size_type pos = npos) const
        noexcept(::std::is_nothrow_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>)
    {
        ::std::basic_string_view<CharT, Traits> tsv = t;

        return view_().find_last_not_of(tsv, pos);
    }
};

template <::std::input_iterator InputIterator, string_detail::mini_alloc Alloc = ::std::allocator<
                                                   typename ::std::iterator_traits<InputIterator>::value_type>>
basic_string(InputIterator, InputIterator, Alloc = Alloc())
    -> basic_string<typename ::std::iterator_traits<InputIterator>::value_type,
                    ::std::char_traits<typename ::std::iterator_traits<InputIterator>::value_type>, Alloc>;

template <::std::ranges::input_range R,
          string_detail::mini_alloc Alloc = ::std::allocator<::std::ranges::range_value_t<R>>>
basic_string(::std::from_range_t, R &&, Alloc = Alloc())
    -> basic_string<::std::ranges::range_value_t<R>, ::std::char_traits<::std::ranges::range_value_t<R>>, Alloc>;

template <class CharT, class Traits, string_detail::mini_alloc Alloc = ::std::allocator<CharT>>
explicit basic_string(::std::basic_string_view<CharT, Traits>, const Alloc & = Alloc())
    -> basic_string<CharT, Traits, Alloc>;

template <class CharT, class Traits, string_detail::mini_alloc Alloc = ::std::allocator<CharT>>
basic_string(::std::basic_string_view<CharT, Traits>, ::std::size_t, ::std::size_t, const Alloc & = Alloc())
    -> basic_string<CharT, Traits, Alloc>;

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs, const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(lhs.get_allocator())};
    r.reserve(lhs.size() + rhs.size());
    r.append(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs, const CharT *rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(lhs.get_allocator())};
    auto rsize = Traits::length(rhs);
    r.reserve(lhs.size() + rsize);
    r.append(lhs);
    r.append(rhs, rsize);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs, const CharT rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(lhs.get_allocator())};
    r.reserve(lhs.size() + 1uz);
    r.append(lhs);
    r.push_back(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs,
    ::std::type_identity_t<::std::basic_string_view<CharT, Traits>> rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(lhs.get_allocator())};
    r.reserve(lhs.size() + rhs.size());
    r.append(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const CharT *lhs, const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(rhs.get_allocator())};
    auto lsize = Traits::length(lhs);
    r.reserve(lsize + rhs.size());
    r.append(lhs, lsize);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    CharT lhs, const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(rhs.get_allocator())};
    r.reserve(1uz + rhs.size());
    r.push_back(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    ::std::type_identity_t<::std::basic_string_view<CharT, Traits>> lhs,
    const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(rhs.get_allocator())};
    r.reserve(lhs.size() + rhs.size());
    r.append(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(bizwen::basic_string<CharT, Traits, Alloc> &&lhs,
                                                                      bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    lhs.append(rhs);

    return ::std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    bizwen::basic_string<CharT, Traits, Alloc> &&lhs, const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    lhs.append(rhs);

    return ::std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(bizwen::basic_string<CharT, Traits, Alloc> &&lhs,
                                                                      const CharT *rhs)
{
    lhs.append(rhs);

    return ::std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(bizwen::basic_string<CharT, Traits, Alloc> &&lhs,
                                                                      const CharT rhs)
{
    lhs.push_back(rhs);

    return ::std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    bizwen::basic_string<CharT, Traits, Alloc> &&lhs,
    ::std::type_identity_t<::std::basic_string_view<CharT, Traits>> rhs)
{
    lhs.append(rhs);

    return ::std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs, bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    rhs.insert(0uz, lhs);

    return ::std::move(rhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(const CharT *lhs,
                                                                      bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    rhs.insert(0uz, lhs);
    return std::move(rhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(const CharT lhs,
                                                                      bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    rhs.insert(rhs.begin(), lhs);
    return std::move(rhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    ::std::type_identity_t<::std::basic_string_view<CharT, Traits>> lhs,
    bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    rhs.insert(0uz, lhs);

    return ::std::move(rhs);
}

template <class CharT, class Traits, class Alloc, class U = CharT>
inline constexpr typename basic_string<CharT, Traits, Alloc>::size_type erase(basic_string<CharT, Traits, Alloc> &c,
                                                                              const U &value)
{
    auto const r = c.end() - ::std::remove(c.begin(), c.end(), value);
    c.resize(c.size() - r);

    return r;
}

template <class CharT, class Traits, class Alloc, class Pred>
inline constexpr typename basic_string<CharT, Traits, Alloc>::size_type erase_if(basic_string<CharT, Traits, Alloc> &c,
                                                                                 Pred pred)
{
    auto const r = c.end() - ::std::remove_if(c.begin(), c.end(), pred);
    c.resize(c.size() - r);

    return r;
}

template <class CharT, class Traits = ::std::char_traits<CharT>>
using basic_string_view = ::std::basic_string_view<CharT, Traits>;

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
using u8string_view = basic_string_view<char8_t>;
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;

using string = bizwen::basic_string<char>;
using wstring = bizwen::basic_string<wchar_t>;
using u8string = bizwen::basic_string<char8_t>;
using u16string = bizwen::basic_string<char16_t>;
using u32string = bizwen::basic_string<char32_t>;

#if !defined(NDEBUG)
static_assert(sizeof(string) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(wstring) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(u8string) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(u16string) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(u32string) == sizeof(char8_t *) * 4uz);
static_assert(::std::contiguous_iterator<string::iterator>);
#endif

namespace pmr
{
template <class CharT, class Traits = ::std::char_traits<CharT>>
using basic_string = ::bizwen::basic_string<CharT, Traits, ::std::pmr::polymorphic_allocator<CharT>>;

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
using u8string = basic_string<char8_t>;
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;
} // namespace pmr
} // namespace bizwen

namespace std
{
template <typename CharT, typename Traits, typename Alloc>
    requires(::std::is_same_v<Traits, ::std::char_traits<CharT>> &&
             (::std::is_same_v<CharT, char> || ::std::is_same_v<CharT, wchar_t> ||
              ::std::is_same_v<CharT, char8_t> || ::std::is_same_v<CharT, char16_t> ||
              ::std::is_same_v<CharT, char32_t>))
struct hash<bizwen::basic_string<CharT, Traits, Alloc>>
{
    constexpr ::std::size_t operator()(bizwen::basic_string<CharT, Traits, Alloc> const &str) const noexcept
    {
        using view = ::std::basic_string_view<CharT, Traits>;
        ::std::hash<view> hasher;

        return hasher(view(str));
    }
};
} // namespace std

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif

#endif // BIZWEN_BASIC_STRING_HPP
















