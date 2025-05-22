#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstring>
#include <cwchar>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <version>

#if !defined(__cpp_if_consteval) && (__cpp_if_consteval >= 202106L)
#error "requires __cpp_if_consteval"
#endif

#if !defined(__cpp_size_t_suffix) && (__cpp_size_t_suffix >= 202011L)
#error "requires __cpp_size_t_suffix"
#endif

namespace bizwen
{
template <typename CharT, class Traits = ::std::char_traits<CharT>, class Allocator = ::std::allocator<CharT>>
class alignas(alignof(int *)) basic_string
{
    static_assert(::std::is_same_v<char, CharT> || ::std::is_same_v<wchar_t, CharT> ||
                  ::std::is_same_v<char8_t, CharT> || ::std::is_same_v<char16_t, CharT> ||
                  ::std::is_same_v<char32_t, CharT>);

  private:
    /**
     * @brief type of long string
     */
    struct ls_type_
    {
        CharT *begin_{};
        CharT *end_{};
        CharT *last_{};
    };

    // -2 is due to the null terminator and size_flag
    /**
     * @brief short_str_max_ is the max length of short string
     */
    static inline constexpr ::std::size_t short_str_max_{sizeof(CharT *) * 4uz / sizeof(CharT) - 2uz};

    /**
     * @brief union storage long string and short string
     */
#pragma pack(1)
    union storage_type_ {
        ::std::array<CharT, short_str_max_ + 1uz /* null terminator */> ss_;
        ls_type_ ls_;
    };

    // https://github.com/microsoft/STL/issues/1364
#if __has_cpp_attribute(msvc::no_unique_address)
    [[msvc::no_unique_address]] Allocator allocator_{};
#else
    [[no_unique_address]] Allocator allocator_{};
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
    alignas(alignof(CharT)) unsigned char size_flag_{};

    // sizeof(basic_string<T>) is always equal to sizeof(void*) * 4

    using atraits_t_ = ::std::allocator_traits<Allocator>;

    /**
     * @return std::out_of_range
     */
    static constexpr auto out_of_range() noexcept
    {
        return ::std::out_of_range("pos/index is out of range, please check it.");
    }

    /**
     * @brief check if the two ranges overlap, and if overlap, new memory needs to allocate
     **/
    static constexpr bool overlap(CharT *first_inner, CharT *last_inner, CharT *first_outer, CharT *last_outer) noexcept
    {
        if !consteval
        {
            // [utilities.comparisons.general] strict total order over pointers.
            ::std::ranges::greater greater;
            ::std::ranges::less less;

            if (less(first_inner, first_outer) || greater(first_inner, last_outer))
            {
                assert(("input range overlaps with *this but is larger.",
                        less(last_inner, first_outer) || greater(last_inner, last_outer)));

                return false;
            }
        }
        // always assume overlap during constant evaluation
        return true;
    }

    constexpr ls_type_ &long_stor_() const noexcept
    {
        return stor_.ls_;
    }

    /**
     * @brief convert to long string and set the flag
     */
    constexpr void long_stor(ls_type_ const &ls) noexcept
    {
        long_stor_() = ls;
        size_flag_ = static_cast<unsigned char>(-1);
        *ls.end_ = CharT{};
    }

    /**
     * @brief convert to short string and resize
     */
    constexpr void short_stor(::std::size_t size) noexcept
    {
        stor_.ss_ = decltype(stor_.ss_)();
        size_flag_ = static_cast<unsigned char>(size);
    }

    constexpr auto &short_stor_() const noexcept
    {
        return stor_.ss_;
    }

    constexpr bool is_long_() const noexcept
    {
        return size_flag_ == (unsigned char)(-1);
    }

    constexpr bool is_short_() const noexcept
    {
        return !is_long_();
    }

    constexpr void resize_shrink_(bool is_long, ::std::size_t n) noexcept
    {
        if (is_long)
        {
            stor_.ls_.end_ = stor_.ls_.begin_ + n;
            *stor_.ls_.end_ = CharT{};
        }
        else
        {
            size_flag_ = static_cast<unsigned char>(n);
            stor_.ss_[size_flag_] = CharT{};
        }
    }

    constexpr ::std::size_t size_() const noexcept
    {
        return is_short_() ? size_flag_ : long_stor_().end_ - long_stor_().begin_;
    }

  public:
    using traits_type = Traits;
    using value_type = CharT;
    using allocator_type = Allocator;
    using size_type = typename ::std::allocator_traits<Allocator>::size_type;
    using difference_type = typename ::std::allocator_traits<Allocator>::difference_type;
    using reference = value_type &;
    using const_reference = value_type const &;
    using pointer = typename ::std::allocator_traits<Allocator>::pointer;
    using const_pointer = typename ::std::allocator_traits<Allocator>::const_pointer;

    static inline constexpr size_type npos = size_type(-1);

    // ********************************* begin volume ******************************

    constexpr ::std::size_t size() const noexcept
    {
        return size_();
    }

    constexpr ::std::size_t length() const noexcept
    {
        return size_();
    }

    constexpr bool empty() const noexcept
    {
        return !size_();
    }

    /**
     * @return size_type{ -1 } / sizeof(CharT) / 2uz - 1uz
     */
    constexpr size_type max_size() const noexcept
    {
        return size_type{-1} / sizeof(CharT) / 2uz - 1uz /* null terminator */;
    }

    constexpr size_type capacity() const noexcept
    {
        return is_short_() ? short_str_max_ : long_stor_().last_ - long_stor_().begin_;
    }

    /**
     * @brief shrink only occur on short string is enough to storage
     */
    constexpr void shrink_to_fit() noexcept
    {
        if (is_long_() && size_() <= short_str_max_)
        {
            // copy to local
            auto const ls = long_stor_();
            short_stor(size_());
            ::std::ranges::copy(ls.begin_, ls.end_ + 1uz /* null terminator */, short_stor_().data());
            dealloc_(true, ls);
        }
    }

    // ********************************* begin element access ******************************

  private:
    /**
     * @return a pointer to the first element
     */
    constexpr CharT const *begin_() const noexcept
    {
        return is_short_() ? short_stor_().begin() : long_stor_().begin_;
    }

    /**
     * @return a pointer to the first element
     */
    constexpr CharT *begin_() noexcept
    {
        return is_short_() ? short_stor_().begin() : long_stor_().begin_;
    }

    /**
     * @return a pointer to the next position of the last element
     */
    constexpr CharT const *end_() const noexcept
    {
        return is_short_() ? short_stor_().begin() + short_size() : long_stor_().end_;
    }

    /**
     * @return a pointer to the next position of the last element
     */
    constexpr CharT *end_() noexcept
    {
        return is_short_() ? short_stor_().begin() + short_size() : long_stor_().end_;
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
        return pos < size_() ? *(begin_() + pos) : throw out_of_range();
    }

    constexpr reference at(size_type pos)
    {
        return pos < size_() ? *(begin_() + pos) : throw out_of_range();
    }

    constexpr const_reference operator[](size_type pos) const noexcept
    {
        assert(("pos >= size.", pos < size_()));

        return *(begin_() + pos);
    }

    constexpr reference operator[](size_type pos) noexcept
    {
        return const_cast<CharT &>(const_cast<basic_string const &>(*this)[pos]);
    }

    constexpr const CharT &front() const noexcept
    {
        assert(("string is empty.", !empty()));

        return *begin_();
    }

    constexpr CharT &front()
    {
        assert(("string is empty.", !empty()));

        return *begin_();
    }

    constexpr const CharT &back() const noexcept
    {
        assert(("string is empty", !empty()));

        return *(end_() - 1uz);
    }

    constexpr CharT &back()
    {
        assert(("string is empty", !empty()));

        return *(end_() - 1uz);
    }

    constexpr operator ::std::basic_string_view<CharT, Traits>() const noexcept
    {
        return ::std::basic_string_view<CharT, Traits>(begin_(), end_());
    }

    // ********************************* begin iterator type ******************************

  private:
    struct iterator_type_
    {
        using difference_type = ::std::ptrdiff_t;
        using value_type = CharT;
        using pointer = ::std::add_pointer_t<value_type>;
        using reference = ::std::add_lvalue_reference_t<CharT>;
        using iterator_category = ::std::random_access_iterator_tag;
        using iterator_concept = ::std::contiguous_iterator_tag;

      private:
        CharT *current_{};

        // the C++ standard states that all iterators of string become invalid after move,
        // so the address of the string can be stored directly.
#ifndef NDEBUG
        basic_string *target_{};
#endif

        // Call after iteration.
        constexpr void check() const noexcept
        {
#ifndef NDEBUG
            // if current_ is not nullptr, then target must not be nullptr.
            assert(("iterator is invalidated", //
                    current_ && current_ <= target_->end_() && current_ >= target_->begin_()));
#endif
        }

        friend class basic_string;

      public:
        iterator_type_() noexcept = default;
        iterator_type_(iterator_type_ const &) noexcept = default;
        iterator_type_(iterator_type_ &&) noexcept = default;
        iterator_type_ &operator=(iterator_type_ const &) & noexcept = default;
        iterator_type_ &operator=(iterator_type_ &&) & noexcept = default;

      private:
#ifndef NDEBUG
        constexpr iterator_type_(CharT *current, basic_string *target) noexcept : current_(current), target_(target)
        {
        }
#else
        constexpr iterator_type_(CharT *current) noexcept : current_(current)
        {
        }
#endif

      public:
        constexpr iterator_type_ operator+(difference_type n) const & noexcept
        {
            auto temp = *this;
            temp.current_ += n;
            temp.check();

            return temp;
        }

        constexpr iterator_type_ operator-(difference_type n) const & noexcept
        {
            auto temp = *this;
            temp.current_ -= n;
            temp.check();

            return temp;
        }

        constexpr friend iterator_type_ operator+(difference_type n, iterator_type_ const &rhs) noexcept
        {
            auto temp = rhs;
            temp.current_ += n;
            temp.check();

            return temp;
        }

        constexpr friend iterator_type_ operator-(difference_type n, iterator_type_ const &rhs) noexcept
        {
            auto temp = rhs;
            temp.current_ -= n;
            temp.check();

            return temp;
        }

        constexpr friend difference_type operator-(iterator_type_ const &lhs, iterator_type_ const &rhs) noexcept
        {
            assert(("iterator belongs to different strings", lhs.target_ == rhs.target_));

            return lhs.current_ - rhs.current_;
        }

        constexpr iterator_type_ &operator+=(difference_type n) & noexcept
        {
            current_ += n;
            check();
            return *this;
        }

        constexpr iterator_type_ &operator-=(difference_type n) & noexcept
        {
            current_ -= n;
            check();
            return *this;
        }

        constexpr iterator_type_ &operator++() & noexcept
        {
            ++current_;
            check();
            return *this;
        }

        constexpr iterator_type_ &operator--() & noexcept
        {
            --current_;
            check();
            return *this;
        }

        constexpr iterator_type_ operator++(int) & noexcept
        {
            iterator_type_ temp{*this};
            ++current_;
            check();
            return temp;
        }

        constexpr iterator_type_ operator--(int) & noexcept
        {
            iterator_type_ temp{*this};
            --current_;
            check();
            return temp;
        }

        constexpr CharT &operator[](difference_type n) const noexcept
        {
#ifndef NDEBUG
            iterator_type_ end = (*this) + n;
            end.check();
#endif
            return *(current_ + n);
        }

        constexpr CharT &operator*() const noexcept
        {
            return *current_;
        }

        constexpr CharT *operator->() const noexcept
        {
            return current_;
        }

        friend constexpr ::std::strong_ordering operator<=>(iterator_type_ const &,
                                                            iterator_type_ const &) noexcept = default;
    };

    // ********************************* begin iterator function ******************************

  public:
    using iterator = iterator_type_;
    using const_iterator = ::std::basic_const_iterator<iterator_type_>;
    using reverse_iterator = ::std::reverse_iterator<iterator>;
    using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

    constexpr iterator begin() noexcept
    {
#ifndef NDEBUG
        return {begin_(), this};
#else
        return {begin_()};
#endif
    }

    constexpr iterator end() noexcept
    {
#ifndef NDEBUG
        return iterator_type_{end_(), this};
#else
        return iterator_type_{end_()};
#endif
    }

    constexpr const_iterator begin() const noexcept
    {
#ifndef NDEBUG
        return iterator_type_{begin_(), const_cast<basic_string *>(this)};
#else
        return iterator_type_{begin_()};
#endif
    }

    constexpr const_iterator end() const noexcept
    {
#ifndef NDEBUG
        return iterator_type_{end_(), const_cast<basic_string *>(this)};
#else
        return iterator_type_{end_()};
#endif
    }

    constexpr const_iterator cbegin() noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() noexcept
    {
        return end();
    }

    // ********************************* begin memory management ******************************

  private:
    /**
     * @brief allocates memory and automatically adds 1 to store null terminator
     * @param n, expected number of characters
     */
    constexpr ls_type_ allocate_(size_type cap, size_type size)
    {
#if defined(__cpp_lib_allocate_at_least) && (__cpp_lib_allocate_at_least >= 202302L)
        auto const [ptr, count] = atraits_t_::allocate_at_least(allocator_, cap + 1uz /* null terminator */);
        return {ptr, ptr + size, ptr + count - 1uz /* null terminator */};
#else
        auto const ptr = atraits_t_::allocate(allocator_, cap + 1uz /* null terminator */);
        return {ptr, ptr + size, ptr + cap};
#endif
    }

    /**
     * @brief dealloc the memory of long string
     * @brief static member function
     * @param ls, allocated long string
     */
    constexpr void dealloc_(bool is_long, ls_type_ const &ls) noexcept
    {
        if (is_long)
            atraits_t_::deallocate(allocator_, ls.begin_, ls.last_ - ls.begin_ + 1uz /* null terminator */);
    }

    /**
     * @brief caculate the length of c style string
     * @param begin begin of characters
     * @return length
     */
    constexpr static size_type c_string_length_(CharT const *begin) noexcept
    {
        if !consteval
        {
            if constexpr (::std::is_same_v<char, CharT> || ::std::is_same_v<char8_t, CharT>)
                return ::std::strlen(reinterpret_cast<char const *>(begin));
            else if constexpr (::std::is_same_v<wchar_t, CharT>)
                return ::std::wcslen(begin);
        }
        else
        {
            auto end = begin;

            for (; *end != CharT{}; ++end)
                ;

            return end - begin;
        }
    }

    /**
     * @brief assign characters to *this
     * @brief this function can be called with any legal state
     * @brief strong exception safety guarantee
     */
    constexpr void assign_(CharT const *first, CharT const *last)
    {
        auto const new_size = static_cast<size_type>(last - first);
        auto const is_long = is_long_();

        // there is no need to worry about whether the ranges overlap,
        // because std::ranges::copy will handle it correctly
        if (capacity() >= new_size)
        {
            if (auto const begin = begin_(); begin != first)
                ::std::ranges::copy(first, last, begin);

            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(first, last, ls.begin_);
            dealloc_(is_long, long_stor_());
            long_stor_(ls);
        }
    }

    /**
     * @brief insert characters to *this
     * @brief this function can be called with any legal state
     * @brief strong exception safety guarantee
     */
    constexpr void insert_(size_type index, CharT const *first, CharT const *last)
    {
        if (index > size_())
            throw out_of_range();

        auto const length = static_cast<size_type>(last - first);
        auto const new_size = size_() + length;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() > new_size && !overlap(first, last, begin, end))
        {
            ::std::ranges::copy_backward(begin + index, end, end + length);
            ::std::ranges::copy(first, last, begin + index);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + index, ls.begin_);
            ::std::ranges::copy(begin + index, end, ls.begin + index + length);
            ::std::ranges::copy(first, last, ls.begin_ + index);
            dealloc_(is_long, long_stor_());
            long_stor_(ls);
        }
    }

    constexpr void replace_(size_type pos, size_type count, CharT const *first, CharT const *last)
    {
        if (pos > size_())
            throw out_of_range();

        count = ::std::ranges::min(size_() - pos, count);

        auto const length = static_cast<size_type>(last - first);
        auto const new_size = size_() - count + length;
        auto begin = begin_();
        auto end = end_();
        auto const is_long = is_long_();

        if (capacity() < new_size && !overlap(first, last, begin, end))
        {
            if (count < length)
                ::std::ranges::copy(begin + pos + count, end, begin + pos + length);
            else
                ::std::ranges::copy_backward(begin + pos + count, end, begin + new_size);

            ::std::ranges::copy(first, last, begin + pos);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + pos, ls.begin_);
            ::std::ranges::copy(first, last, ls.begin_ + pos);
            ::std::ranges::copy(begin + pos + count, end, ls.begin_ + pos + count);
            dealloc_(is_long, long_stor_());
            long_stor_(ls);
        }
    }

    constexpr void reserve_(size_type new_cap)
    {
        assert(new_cap > capacity());
        auto const size = size_();
        auto begin = begin_();
        auto end = end_();
        auto const is_long = is_long_();
        auto const ls = allocate_(new_cap, size);
        ::std::ranges::copy(begin, end, ls.begin_);
        dealloc_(is_long, long_stor_());
        long_stor_(ls);
    }

  public:
    constexpr ~basic_string()
    {
        dealloc_(is_long_(), stor_.ls_);
    }

    /**
     * @brief reserve memory
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param new_cap new capacity
     */
    constexpr void reserve(size_type new_cap)
    {
        if (new_cap <= capacity())
            return;
        reserve_(new_cap);
    }

    /**
     * @brief resize string length
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param count new size
     * @param ch character to fill
     */
    constexpr void resize(size_type count, CharT ch)
    {
        auto const size = size_();
        auto const is_long = is_long_();

        if (count <= size)
            return resize_shrink_(is_long, count);

        reserve(count);
        ::std::ranges::fill(end_(), end_() + count - size, ch);
        resize_shrink_(is_long, count);
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
            reserve_(size * 2uz - size / 2uz);

        *end_() = ch;
        resize_shrink_(size + 1uz);
    }

    // ********************************* begin swap ******************************

  private:
    constexpr void swap_without_ator(basic_string &other) noexcept
    {
        auto &self = *this;
        ::std::ranges::swap(self.stor_, other.stor_);
        ::std::ranges::swap(self.size_flag_, other.size_flag_);
    }

  public:
    constexpr void swap(basic_string &other) noexcept
    {
        if constexpr (atraits_t_::propagate_on_container_swap::value)
            ::std::ranges::swap(other.allocator_, other.allocator_);
        else
            assert(other.allocator_ == allocator_);

        other.swap_without_ator(*this);
    }

    friend void swap(basic_string &lhs, basic_string &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // ********************************* begin constructor ******************************

    constexpr void construct_(::std::size_t n)
    {
        if (short_str_max_ >= n)
        {
            long_stor_(allocate_(n, n));
        }
        else
        {
            resize_shrink_(false, n);
        }
    }

  public:
    constexpr basic_string() noexcept = default;

    constexpr basic_string(allocator_type const &a) noexcept : allocator_(a)
    {
    }

    constexpr basic_string(size_type n, CharT ch, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        construct_(n);
        ::std::ranges::fill(begin_(), end_(), ch);
    }

    constexpr basic_string(const basic_string &other, size_type pos, size_type count,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        if (pos > other.size_())
            throw out_of_range();

        count = ::std::ranges::min(other.size_() - pos, count);

        construct_(count);
        ::std::ranges::copy(other.begin_() + pos, other.begin_() + pos + count, begin_());
    }

    constexpr basic_string(const basic_string &other, size_type pos, allocator_type const &a = allocator_type())
        : basic_string(other, pos, npos /* end */, a)
    {
    }

    constexpr basic_string(const CharT *s, size_type count, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        construct_(count);
        ::std::ranges::copy(s, s + count, begin_());
    }

    constexpr basic_string(const CharT *s, allocator_type const &a = allocator_type())
        : basic_string(s, c_string_length_(s), a)
    {
    }

    template <class U, class V>
        requires ::std::input_iterator<U>
    constexpr basic_string(U first, V last, const Allocator &alloc = Allocator()) : allocator_(alloc)
    {
        if constexpr (::std::random_access_iterator<U>)
        {
            construct_(last - first);
            ::std::ranges::copy(first, last, begin_());
        }
        else
        {
            for (; first != last; ++first)
                push_back(*first);
        }
    }

    constexpr basic_string(const basic_string &other)
        : allocator_(atraits_t_::select_on_container_copy_construction(other.allocator_))
    {
        construct_(other.size_());
        ::std::ranges::copy(other.begin_(), other.end_(), begin_());
    }

    constexpr basic_string(const basic_string &other, allocator_type const &a) : allocator_(a)
    {
        construct_(other.size_());
        ::std::ranges::copy(other.begin_(), other.end_(), begin_());
    }

    constexpr basic_string(basic_string &&other, size_type pos, size_type count,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        if (pos > other.size_())
            throw out_of_range();

        count = ::std::ranges::min(other.size_() - pos, count);

        if (other.allocator_ == a)
        {
            if (pos != 0uz)
                ::std::ranges::copy(other.begin_() + pos, other.begin_() + pos + count, other.begin_());

            stor_ = other.stor_;
            resize_shrink_(other.is_long_(), count);
            other.short_stor(0uz);
        }
        else
        {
            construct_(other.size_());
            ::std::ranges::copy(other.begin_() + pos, other.begin_() + pos + count, begin_());
            other.dealloc_(other.is_long_(), other.long_stor_());
        }
    }

    constexpr basic_string(basic_string &&other) noexcept : allocator_(other.allocator_)
    {
        other.swap_without_ator(*this);
    }

    constexpr basic_string(basic_string &&other, allocator_type const &a) : allocator_(a)
    {
        if (other.allocator_ == a)
        {
            other.swap_without_ator(*this);
        }
        else
        {
            construct_(other.size_());
            ::std::ranges::copy(other.begin_(), other.end_(), begin_());
        }
    }

    constexpr basic_string(::std::initializer_list<CharT> ilist, allocator_type const &a = allocator_type())
        : basic_string(ilist.begin(), ilist.size(), a)
    {
    }

    // clang-format off
    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    constexpr basic_string(const StringViewLike& t, allocator_type const& a = allocator_type())
	     : basic_string(::std::basic_string_view<CharT, Traits>{ t }.begin(), ::std::basic_string_view<CharT, Traits>{ t }.size(), a)
    {
    }

    // clang-format on

    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>>
    constexpr basic_string(const StringViewLike &t, size_type pos, size_type n,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;

        if (pos > sv.size())
            throw out_of_range();

        n = ::std::ranges::min(sv.size() - pos, n);
        construct_(n);
        ::std::ranges::copy(sv.data() + pos, sv.data() + pos + n, begin_());
    }

#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
    // tagged constructors to construct from container compatible range
    template <class R>
        requires ::std::ranges::range<R> && requires {
            typename R::value_type;
            requires ::std::same_as<typename R::value_type, CharT>;
        }
    constexpr basic_string(::std::from_range_t, R &&rg, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        auto first = ::std::ranges::begin(rg);
        auto last = ::std::ranges::end(rg);

        if constexpr (::std::ranges::sized_range<R>)
        {
            construct_(::std::ranges::size(rg));
            ::std::ranges::copy(first, last, begin_());
        }
        else
        {
            for (; first != last; ++first)
                push_back(*first);
        }
    }
#endif

    // ********************************* begin append ******************************

    template <class InputIt>
        requires ::std::input_iterator<InputIt>
    constexpr basic_string &append(InputIt first, InputIt last)
    {
        if constexpr (::std::contiguous_iterator<InputIt>)
        {
            append_(::std::to_address(first), ::std::to_address(last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            append_(temp.begin_(), temp.end_());
        }

        return *this;
    }

    // ********************************* begin assign ******************************

    constexpr basic_string &assign(size_type count, CharT ch)
    {
        reserve(count);
        auto const is_long = is_long_();
        ::std::ranges::fill(begin_(), end_(), ch);
        resize_shrink_(is_long, count);

        return *this;
    }

  private:
    constexpr void copy_ator_and_move(basic_string &str) noexcept
    {
        dealloc_(is_long_(), long_stor_());
        stor_.ls_ = str.stor_.ls_;
        size_flag_ = str.size_flag_;
        str.stor_.ls_ = ls_type_();
        str.size_flag_ = 0u;
    }

    constexpr basic_string &assign(const basic_string &str)
    {

        if (&str == this)
            return *this;

        if constexpr (atraits_t_::propagate_on_container_copy_assignment::value)
        {
            if (allocator_ != str.allocator_)
            {
                basic_string temp{str};
                copy_ator_and_move(temp);

                return *this;
            }
        }

        assign_(str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &assign(const basic_string &str, size_type pos, size_type count = npos)
    {
        if (pos > str.size_())
            throw out_of_range();

        count = ::std::ranges::min(str.size_() - pos, count);
        assign_(str.begin_() + pos, str.begin_() + pos + count);

        return *this;
    }

    constexpr basic_string &assign(basic_string &&str) noexcept
    {
        if constexpr (atraits_t_::propagate_on_container_move_assignment::value)
        {
            copy_ator_and_move(str);
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

    constexpr basic_string &assign(const CharT *s, size_type count)
    {
        assign_(s, s + count);

        return *this;
    }

    constexpr basic_string &assign(const CharT *s)
    {
        assign_(s, s + c_string_length_(s));

        return *this;
    }

    template <class InputIt>
        requires ::std::input_iterator<InputIt>
    constexpr basic_string &assign(InputIt first, InputIt last)
    {
        if constexpr (::std::contiguous_iterator<InputIt>)
        {
            assign_(::std::to_address(first), ::std::to_address(last));
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

    // clang-format off
    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    basic_string& assign(const StringViewLike& t)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;
        assign_(sv.data(), sv.data()+sv.size());

        return *this;
    }

    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    constexpr basic_string& assign(const StringViewLike& t, size_type pos, size_type count = npos)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;

        if (pos > sv.size())
            throw out_of_range();

        count = ::std::ranges::min(sv.size() - pos, count);
        assign_(sv.data() + pos, sv.data() + pos + count);

        return *this;
    }
    // clang-format on

    // ********************************* begin operator= ******************************

    basic_string(::std::nullptr_t) = delete;
    constexpr basic_string &operator=(::std::nullptr_t) = delete;

    constexpr basic_string &operator=(basic_string &&other) noexcept(
        ::std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
        ::std::allocator_traits<Allocator>::is_always_equal::value)
    {
        return assign(::std::move(other));
    }

    constexpr basic_string &operator=(const basic_string &str)
    {
        return assign(str);
    }

    constexpr basic_string &operator=(const CharT *s)
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

    // clang-format off
    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    constexpr basic_string& operator=(const StringViewLike& t)
    {
        return assign(t);
    }
    // clang-format on

    // ********************************* begin compare ******************************

    friend constexpr ::std::strong_ordering operator<=>(basic_string const &lhs, basic_string const &rhs) noexcept
    {
        auto const lsize = lhs.size_();
        auto const rsize = rhs.size_();

        for (auto begin = lhs.begin_(), end = begin + ::std::ranges::min(lsize, rsize), start = rhs.begin_();
             begin != end; ++begin, ++start)
        {
            if (*begin > *start)
                return ::std::strong_ordering::greater;
            else if (*begin < *start)
                return ::std::strong_ordering::less;
        }

        if (lsize > rsize)
            return ::std::strong_ordering::greater;
        else if (lsize < rsize)
            return ::std::strong_ordering::less;

        return ::std::strong_ordering::equal;
    }

    friend constexpr ::std::strong_ordering operator<=>(basic_string const &lhs, CharT const *rhs) noexcept
    {
        auto start = rhs;
        auto rsize = basic_string::c_string_length_(start);
        auto lsize = lhs.size_();

        for (auto begin = lhs.begin_(), end = begin + ::std::ranges::min(lsize, rsize); begin != end; ++begin, ++start)
        {
            if (*begin > *start)
                return ::std::strong_ordering::greater;
            else if (*begin < *start)
                return ::std::strong_ordering::less;
        }

        if (lsize > rsize)
            return ::std::strong_ordering::greater;
        else if (lsize < rsize)
            return ::std::strong_ordering::less;
        else
            return ::std::strong_ordering::equal;
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
                if (*first != *begin)
                {
                    return false;
                }
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

    friend constexpr bool operator==(basic_string const &lhs, CharT const *rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs, rhs + c_string_length_(rhs));
    }

    // ********************************* begin append ******************************

  private:
    constexpr void append_(CharT const *first, CharT const *last)
    {
        auto const length = static_cast<size_type>(last - first);
        auto const size = size_();
        auto const begin = begin_();
        auto const end = end_();
        auto const new_size = size + length;
        auto const is_long = is_long_();

        if (capacity() >= new_size && !overlap(first, last, begin, end))
        {
            ::std::ranges::copy(first, last, end);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, end, ls.begin_);
            ::std::ranges::copy(first, last, ls.begin_ + size);
            dealloc_(is_long, long_stor_());
            long_stor(ls);
        }
    }

  public:
    constexpr basic_string &append(size_type count, CharT ch)
    {
        auto const size = size_();
        auto const is_long = is_long_();
        reserve(size + count);
        ::std::ranges::fill(end_(), end_() + count, ch);
        resize_shrink_(is_long, size + count);

        return *this;
    }

    constexpr basic_string &append(const CharT *s, size_type count)
    {
        append_(s, s + count);

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
            throw out_of_range();

        count = ::std::ranges::min(str.size_() - pos, count);

        append_(str.begin_() + pos, str.begin_() + pos + count);

        return *this;
    }

    constexpr basic_string &append(const CharT *s)
    {
        append_(s, s + c_string_length_(s));

        return *this;
    }

    constexpr basic_string &append(::std::initializer_list<CharT> ilist)
    {
        append_(ilist.begin(), ilist.end());

        return *this;
    }

    // clang-format off
    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    constexpr basic_string& append(const StringViewLike& t)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;
        append_(sv.data(),sv.data()+sv.size());

        return *this;
    }

    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    constexpr basic_string& append(const StringViewLike& t, size_type pos, size_type count = npos)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;

        if (pos > sv.size())
            throw out_of_range();

        count = ::std::ranges::min(sv.size() - pos, count);

        return append_(sv.data() + pos,sv.data() + pos + count);
    }

    // clang-format on

    // ********************************* begin operator+= ******************************

    // clang-format off
    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
    constexpr basic_string& operator+=(const StringViewLike& t)
    {
        return append(t);
    }

    // clang-format on

    constexpr basic_string &operator+=(const basic_string &str)
    {
        return append(str);
    }

    constexpr basic_string &operator+=(CharT ch)
    {
        push_back(ch);

        return *this;
    }

    constexpr basic_string &operator+=(const CharT *s)
    {
        return append(s);
    }

    constexpr basic_string &operator+=(::std::initializer_list<CharT> ilist)
    {
        return append(ilist);
    }

    // ********************************* begin search ******************************

  public:
    constexpr bool starts_with(::std::basic_string_view<CharT, Traits> sv) const noexcept
    {
        return sv.size() <= size_() && equal_(sv.data(), sv.data() + sv.size(), begin_(), begin_() + sv.size());
    }

    constexpr bool starts_with(CharT ch) const noexcept
    {
        return *begin_() == ch;
    }

    constexpr bool starts_with(const CharT *s) const
    {
        auto const length = c_string_length_(s);

        return length <= size_() && equal_(s, s + length, begin_(), begin_() + length);
    }

    constexpr bool ends_with(::std::basic_string_view<CharT, Traits> sv) const noexcept
    {
        return sv.size() <= size_() && equal_(sv.data(), sv.data() + sv.size(), end_() - sv.size(), end_());
    }

    constexpr bool ends_with(CharT ch) const noexcept
    {
        return *(end_() - 1) == ch;
    }

    constexpr bool ends_with(CharT const *s) const
    {
        auto const length = c_string_length_(s);

        return length <= size_() && equal_(s, s + length, end_() - length, end_());
    }

#if defined(__cpp_lib_string_contains) && (__cpp_lib_string_contains >= 202011L)
    constexpr bool contains(::std::basic_string_view<CharT, Traits> sv) const noexcept
    {
        return ::std::basic_string_view<CharT, Traits>{begin_(), end_()}.contains(sv);
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

    constexpr bool contains(const CharT *s) const noexcept
    {
        return ::std::basic_string_view<CharT, Traits>{begin_(), end_()}.contains(
            ::std::basic_string_view<CharT, Traits>{s, s + c_string_length_(s)});
    }
#endif

    // ********************************* begin insert ******************************

    constexpr basic_string &insert(size_type index, size_type count, CharT ch)
    {
        if (index > size_())
            throw out_of_range();

        auto const new_size = size_() + count;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();
        if (capacity() <= new_size)
        {
            ::std::ranges::copy_backward(begin + index, end, end + count);
            ::std::ranges::fill(begin + index, begin + index + count, ch);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            std::ranges::copy(begin, begin + index, ls.begin_);
            std::ranges::copy(begin + index, end, ls.begin_ + index + count);
            std::ranges::fill(ls.begin_ + index, ls.begin_ + index + count, ch);
            dealloc_(is_long, long_stor_());
            long_stor(ls);
        }

        return *this;
    }

    constexpr basic_string &insert(size_type index, const CharT *s, size_type count)
    {
        insert_(index, s, s + count);

        return *this;
    }

    constexpr basic_string &insert(size_type index, const CharT *s)
    {
        insert_(index, s, s + c_string_length_(s));

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
            throw out_of_range();

        count = ::std::ranges::min(str.size_() - s_index, count);
        insert_(index, str.begin_() + s_index, str.begin_() + s_index + count);

        return *this;
    }

    constexpr iterator insert(const_iterator pos, CharT ch)
    {
        assert(("pos isn't in this string.", pos.current_ >= begin_() && pos.current_ <= end_()));

        auto const size = size_();
        auto const index = pos->current_ - begin_();
        reserve(size + 1);
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();
        ::std::ranges::copy_backward(begin + index, end, end + 1);
        *(begin + index) = ch;
        resize_shrink_(is_long, size + 1);

#ifndef NDEBUG
        return {begin + index, this};
#else
        return {start};
#endif
    }

    constexpr iterator insert(const_iterator pos, size_type count, CharT ch)
    {
        auto const index = pos->current_ - begin_();
        insert(index, count, ch);

#ifndef NDEBUG
        return {begin_() + index, this};
#else
        return {begin_() + index};
#endif
    }

    template <class InputIt>
        requires ::std::input_iterator<InputIt>
    constexpr iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        assert(("pos isn't in this string.", pos.base().current_ >= begin_() && pos.base().current_ <= end_()));

        auto const index = pos.base().current_ - begin_();

        if constexpr (::std::contiguous_iterator<InputIt>)
        {
            insert_(index, ::std::to_address(first), ::std::to_address(last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            insert_(index, temp.begin_(), temp.end_());
        }

#ifndef NDEBUG
        return {begin_() + index, this};
#else
        return {begin_() + index};
#endif
    }

    constexpr iterator insert(const_iterator pos, ::std::initializer_list<CharT> ilist)
    {
        auto const index = pos.base().current_ - begin_();

        insert_(index, ilist.begin(), ilist.begin() + ilist.size());

#ifndef NDEBUG
        return {begin_() + index, this};
#else
        return {begin_() + index};
#endif
    }

    // clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& insert(size_type pos, const StringViewLike& t)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            insert_(pos, sv.data(), sv.data() + sv.size());

            return *this;
        }

        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& insert(size_type pos, const StringViewLike& t, size_type t_index, size_type count = npos)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;

            if (t_index > sv.size())
                throw out_of_range();

            count = ::std::ranges::min(sv.size() - t_index, count);
            insert_(pos, sv.data() + t_index, sv.data() + t_index + count);

            return *this;
        }

    // clang-format on

    // ********************************* begin erase ******************************

  private:
    constexpr void erase_(CharT *first, CharT const *last) noexcept
    {
        assert(("first or last is not in this string", first >= begin_() && last <= end_()));

        auto const is_long = is_long_();
        auto const size = size_();
        ::std::ranges::copy(last, end_(), first);
        resize_shrink_(is_long, size - (last - first));
    }

  public:
    constexpr basic_string &erase(size_type index = 0uz, size_type count = npos)
    {
        if (index > size_())
            throw out_of_range();

        count = ::std::ranges::min(size_() - index, count);
        erase_(size_() + index, size_() + index + count);

        return *this;
    }

    constexpr iterator erase(const_iterator position) noexcept
    {
        auto const start = position.base().current_;

        assert(("pos is not in this string", start >= begin_() && start <= end_()));

        auto const is_long = is_long_();
        auto const size = size_();
        ::std::ranges::copy(start + 1uz, end_(), start);
        resize_shrink_(is_long, size - 1uz);

#ifndef NDEBUG
        return {start, this};
#else
        return {start};
#endif
    }

    constexpr iterator erase(const_iterator first, const_iterator last) noexcept
    {
        auto const start = first.base().current_;
        erase_(start, last.base().current_);

#ifndef NDEBUG
        return {start, this};
#else
        return {start};
#endif
    }

    // ********************************* begin pop_back ******************************

    constexpr void pop_back() noexcept
    {
        assert(("string is empty", !empty()));
        resize_(size_() - 1uz);
    }

    // ********************************* begin replace ******************************

    constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str)
    {
        replace_(pos, count, str.begin_(), str.end_());

        return *pos;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, const basic_string &str)
    {
        auto const start = first.base().current_;

        assert(("pos is not in this string.", start >= begin_() && start <= end_()));

        replace_(start - begin_(), last - first, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str, size_type pos2,
                                    size_type count2 = npos)
    {
        if (pos2 > str.size_())
            throw out_of_range();

        count2 = ::std::ranges::min(count2, str.size_() - pos2);
        replace_(pos, count, str.begin_() + count2, str.begin_() + count2 + pos2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, const CharT *cstr, size_type count2)
    {
        replace_(pos, count, cstr, cstr + count2);

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, const CharT *cstr, size_type count2)
    {
        assert(("range is not in this string.", first >= begin() && last <= end()));

        replace_(first.base().current_ - begin_(), last - first, cstr, cstr + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, const CharT *cstr)
    {
        replace_(pos, count, cstr, cstr + c_string_length_(cstr));

        return *pos;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, const CharT *cstr)
    {
        assert(("range is not in this string.", first >= begin() && last <= end()));

        replace_(first.base().current_ - begin_(), last - first, cstr, cstr + c_string_length_(cstr));

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, ::std::initializer_list<CharT> ilist)
    {
        replace_(first.base().current_ - begin_(), last - first, ilist.begin(), ilist.begin() + ilist.size());

        return *this;
    }

    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>> &&
                 (!::std::is_convertible_v<const StringViewLike &, const CharT *>)
    constexpr basic_string &replace(size_type pos, size_type count, const StringViewLike &t)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;
        replace_(pos, count, sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>> &&
                 (!::std::is_convertible_v<const StringViewLike &, const CharT *>)
    constexpr basic_string &replace(const_iterator first, const_iterator last, const StringViewLike &t)
    {
        assert(("range is not in this string.", first >= begin() && last <= end()));

        ::std::basic_string_view<CharT, Traits> sv = t;
        auto sv_data = sv.data();
        replace_(first.base().current_ - begin_(), static_cast<size_type>(last - first), sv.data(),
                 sv.data() + sv.size());

        return *this;
    }

    template <class StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<CharT, Traits>> &&
                 (!::std::is_convertible_v<const StringViewLike &, const CharT *>)
    constexpr basic_string &replace(size_type pos, size_type count, const StringViewLike &t, size_type pos2,
                                    size_type count2 = npos)
    {
        ::std::basic_string_view<CharT, Traits> sv = t;

        if (pos2 > sv.size())
            throw out_of_range();

        count2 = ::std::ranges::min(sv.size() - pos2, count2);
        replace_(pos, count, sv.data() + pos2, sv.data() + pos2 + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, size_type count2, CharT ch)
    {
        if (pos > size_())
            throw out_of_range();

        count = ::std::ranges::min(size_() - pos, count);

        auto const size = size_();
        auto const new_size = size - count + count2;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size)
        {
            if (count < count2)
                ::std::ranges::copy(begin + pos + count, end, begin + pos + count2);
            else
                ::std::ranges::copy_backward(begin + pos + count, end, begin + new_size);

            ::std::ranges::fill(begin + pos, begin + pos + count2, ch);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + pos, ls.begin_);
            ::std::ranges::copy(begin + pos + count, end, ls.begin_ + pos + count2);
            ::std::ranges::fill(ls.begin_ + pos, ls.begin_ + pos + count2, ch);
            dealloc_(is_long, long_stor_());
            long_stor(ls);
        }

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, size_type count2, CharT ch)
    {
        replace(first.base().current_ - begin_(), last - first, count2, ch);

        return *this;
    }

    template <class InputIt>
    constexpr basic_string &replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2)
        requires ::std::input_iterator<InputIt>
    {
        if constexpr (::std::contiguous_iterator<InputIt>)
        {
            replace(first, last, ::std::to_address(first2), last2 - first2);
        }
        else
        {
            basic_string temp{first2, last2, allocator_};
            replace_(first.base().current_ - begin_(), last - first, temp.begin_(), temp.end_());
        }

        return *this;
    }
};

using string = bizwen::basic_string<char8_t>;
using wstring = bizwen::basic_string<wchar_t>;
using u8string = bizwen::basic_string<char8_t>;
using u16string = bizwen::basic_string<char16_t>;
using u32string = bizwen::basic_string<char32_t>;

static_assert(sizeof(string) == sizeof(char8_t *) * 4);
static_assert(sizeof(wstring) == sizeof(char8_t *) * 4);
static_assert(sizeof(u8string) == sizeof(char8_t *) * 4);
static_assert(sizeof(u16string) == sizeof(char8_t *) * 4);
static_assert(sizeof(u32string) == sizeof(char8_t *) * 4);
static_assert(::std::contiguous_iterator<string::iterator>);
} // namespace bizwen
