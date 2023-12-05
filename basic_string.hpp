#include <memory>
#include <string_view>
#include <array>
#include <concepts>
#include <utility>
#include <cassert>
#include <compare>
#include <stdexcept>
#include <cstring>
#include <iterator>

namespace bizwen
{
    template <typename T> struct is_character: std::bool_constant<std::is_same_v<char, std::remove_cv_t<T>> || std::is_same_v<signed char, std::remove_cv_t<T>> || std::is_same_v<unsigned char, std::remove_cv_t<T>> || std::is_same_v<wchar_t, std::remove_cv_t<T>> || std::is_same_v<char8_t, std::remove_cv_t<T>> || std::is_same_v<char16_t, std::remove_cv_t<T>> || std::is_same_v<char32_t, std::remove_cv_t<T>>>
    {
    };

    template <typename T> constexpr bool is_character_v = is_character<T>::value;

    template <typename T>
    concept character = is_character_v<T>;

    template <character CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>> class basic_string
    {
    private:
        /*
         * @brief type of null tag
         */
        enum null_t : char
        {
            null
        };

        /**
         * @brief type of long string
         */
        struct ls_type_
        {
            CharT* begin_{};
            CharT* end_{};
            CharT* last_{};
        };

        /**
         * @brief union storage long string and short string
         */
        union storage_type_ {
            std::array<CharT, sizeof(CharT*) * 4 / sizeof(CharT) - 1> ss_;
            ls_type_ ls_;
        };

        /**
         * https://github.com/microsoft/STL/issues/1364
         */
#ifdef _MSC_VER
        [[msvc::no_unique_address]] Allocator allocator_{};
#else
        [[no_unique_address]] Allocator allocator_{};
#endif // _MSC_VER
        /**
         * @brief storage of string
         */
        storage_type_ stor_{};
        /**
         * @brief flag > 0: short string, length of string is size_flag
         * @brief flag = 0: empty string
         * @brief flag = -1: long string, length of string is end - begin
         * @brief flag = -2: null string
         */
        signed char size_flag_{};

        /**
         * @brief short_string_max_ is the max length of short string
         */
        static inline constexpr std::size_t short_string_max_{ sizeof(storage_type_::ss_) / sizeof(CharT*) - 1 };

        static inline constexpr bool nothrow_move_allocator_ = std::is_nothrow_move_constructible_v<Allocator>;

        constexpr bool is_long_() const noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            return size_flag_ == -1;
        }

        constexpr bool is_short_() const noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            return size_flag_ > 0;
        }

        constexpr bool is_empty_() const noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            return !size_flag_;
        }

        constexpr bool is_null_() const noexcept
        {
            return size_flag_ == -2;
        }

        constexpr std::size_t size_() const noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            if (is_long_())
            {
                auto&& ls = stor_.ls_;
                return ls.end_ - ls.begin_;
            }
            else
            {
                return size_flag_;
            }
        }

    public:
        using traits_type = Traits;
        using value_type = CharT;
        using allocator_type = Allocator;
        using size_type = std::allocator_traits<Allocator>::size_type;
        using difference_type = std::allocator_traits<Allocator>::difference_type;
        using reference = value_type&;
        using const_reference = value_type const&;
        using pointer = std::allocator_traits<Allocator>::pointer;
        using const_pointer = std::allocator_traits<Allocator>::const_pointer;

        static inline constexpr size_type npos = -1;

        constexpr allocator_type get_allocator() const
        {
            return allocator_;
        }

        /**
         * @brief check if string is null
         * @brief the only way to create a null string is through the null_t constructor
         */
        constexpr bool is_null() const noexcept
        {
            return is_null_();
        }

        /**
         * @brief transform null string to empty string
         * @brief operator= and transform is the only legal modification operations for a null string
         */
        constexpr void transform_null_to_empty() noexcept
        {
            assert(("only null string can call ", !is_null_()));
            size_flag_ = 0;
        }

        // ********************************* begin volume ******************************
        constexpr bool empty() const noexcept
        {
            return is_empty_();
        }

        constexpr std::size_t size() const noexcept
        {
            return size_();
        }

        constexpr std::size_t length() const noexcept
        {
            return size_();
        }

        /**
         * @return size_type{ -1 } / 2 on 64-bit arch
         * @return size_type{ -1 } - 2 on 32-bit arch
         */
        constexpr size_type max_size() const noexcept
        {
            if constexpr (sizeof(size_type) > 4)
                return size_type{ -1 } / 2;
            else
                return size_type{ -1 } - 2;
        }

        constexpr size_type capacity() const noexcept
        {
            if (is_long_())
            {
                auto&& ls = stor_.ls_;
                return ls.last_ - ls.begin_ - 1;
            }
            else
            {
                return short_string_max_;
            }
        }

        /**
         * @brief never shrink
         */
        constexpr void shrink_to_fit() const noexcept
        {
            return;
        }

        // ********************************* begin element access ******************************

    private:
        /**
         * @brief internal use
         * @return a pointer to the first element
         */
        constexpr CharT const* begin_() const noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            if (is_long_())
            {
                return stor_.ls_.begin_;
            }
            else
            {
                return stor_.ss_.data();
            }
        }

        /**
         * @brief internal use
         * @return a pointer to the first element
         */
        constexpr CharT* begin_() noexcept
        {
            return const_cast<CharT*>(const_cast<basic_string const&>(*this).begin_());
        }

        /**
         * @brief internal use
         * @return a pointer to the next position of the last element
         */
        constexpr CharT* end_() noexcept
        {
            return begin_() + size_();
        }

        /**
         * @brief internal use
         * @return a pointer to the next position of the last element
         */
        constexpr CharT const* end_() const noexcept
        {
            return begin_() + size_();
        }

    public:
        constexpr CharT const* data() const noexcept
        {
            return begin_();
        }

        constexpr CharT* data() noexcept
        {
            return begin_();
        }

        constexpr CharT const* c_str() const noexcept
        {
            return begin_();
        }

        constexpr const_reference at(size_type pos) const
        {
            if (pos >= size_())
                throw std::out_of_range{ {} };
            return *(begin_() + pos);
        }

        constexpr reference at(size_type pos)
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this).at(pos));
        }

        constexpr const_reference operator[](size_type pos) const noexcept
        {
            assert(("pos >= size, please check the arg", pos < size_()));
            return *(begin_() + pos);
        }

        constexpr reference operator[](size_type pos) noexcept
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this)[pos]);
        }

        constexpr const CharT& front() const noexcept
        {
            assert(("string is empty, cannot call ", !is_empty_()));
            return *begin_();
        }

        constexpr CharT& front()
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this).front());
        }

        constexpr const CharT& back() const noexcept
        {
            assert(("string is empty, cannot call ", !is_empty_()));
            return *(end_() - 1);
        }

        constexpr CharT& back()
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this).back());
        }

        constexpr operator std::basic_string_view<CharT, Traits>() const noexcept
        {
            return std::basic_string_view<CharT, Traits>(begin_(), end_());
        }

        // ********************************* begin swap ******************************
        constexpr void swap(basic_string& other) noexcept(nothrow_move_allocator_)
        {
            assert(("string is null, cannot call ", !is_null_()));
            auto&& self = *this;
            std::ranges::swap(self.allocator_, other.allocator_);
            std::ranges::swap(self.stor_, other.stor_);
            std::ranges::swap(self.size_flag_, other.size_flag_);
        }

        friend void swap(basic_string& self, basic_string& other) noexcept(nothrow_move_allocator_)
        {
            self.swap(other);
        }

        // ********************************* begin iterator type ******************************
    private:
        struct iterator_type_
        {
            using difference_type = std::ptrdiff_t;
            using value_type = CharT;
            using pointer = std::add_pointer_t<value_type>;
            using reference = std::add_lvalue_reference_t<CharT>;
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::contiguous_iterator_tag;

            CharT* current_{};
#ifndef NDEBUG
            basic_string* target_{};
#endif // _DEBUG
            constexpr void check() const noexcept
            {
#ifndef NDEBUG
                if (current_ && current_ <= target_->end_() && current_ >= target_->begin_())
                    ;
                else
                    assert(("iterator is invalidated", false));
#endif // _DEBUG
            }
            friend class basic_string;

            iterator_type_() noexcept = default;
            iterator_type_(iterator_type_ const&) noexcept = default;
            iterator_type_(iterator_type_&&) noexcept = default;
            iterator_type_& operator=(iterator_type_ const&) & noexcept = default;
            iterator_type_& operator=(iterator_type_&&) & noexcept = default;
#ifndef NDEBUG

            constexpr iterator_type_(CharT* current, basic_string* target) : current_(current), target_(target)
            {
            }
#else
            constexpr iterator_type_(CharT* current) : current_(current)
            {
            }
#endif // NDEBUG
            constexpr iterator_type_ operator+(difference_type n) const& noexcept
            {
                auto temp = *this;
                temp.current_ += n;
                temp.check();
                return temp;
            }

            constexpr iterator_type_ operator-(difference_type n) const& noexcept
            {
                auto temp = *this;
                temp.current_ -= n;
                temp.check();
                return temp;
            }

            constexpr friend iterator_type_ operator+(difference_type n, iterator_type_ const& rhs) noexcept
            {
                auto temp = rhs;
                temp.current_ += n;
                temp.check();
                return temp;
            }

            constexpr friend iterator_type_ operator-(difference_type n, iterator_type_ const& rhs) noexcept
            {
                auto temp = rhs;
                temp.current_ -= n;
                temp.check();
                return temp;
            }

            constexpr friend difference_type operator-(iterator_type_ const& lhs, iterator_type_ const& rhs) noexcept
            {
                assert(("iter belongs to different strings", lhs.target_ == rhs.target_));
                return lhs.current_ - rhs.current_;
            }

            constexpr iterator_type_& operator+=(difference_type n) & noexcept
            {
                current_ += n;
                check();
                return *this;
            }

            constexpr iterator_type_& operator-=(difference_type n) & noexcept
            {
                current_ -= n;
                check();
                return *this;
            }

            constexpr iterator_type_& operator++() & noexcept
            {
                ++current_;
                check();
                return *this;
            }

            constexpr iterator_type_& operator--() & noexcept
            {
                --current_;
                check();
                return *this;
            }

            constexpr iterator_type_ operator++(int) const& noexcept
            {
                ++current_;
                check();
                return *this;
            }

            constexpr iterator_type_ operator--(int) const& noexcept
            {
                --current_;
                check();
                return *this;
            }

            constexpr CharT& operator[](difference_type n) const noexcept
            {
#ifndef NDEBUG
                iterator_type_ end = (*this) + n;
                end.check();
#endif
                return *(current_ + n);
            }

            constexpr CharT& operator*() const noexcept
            {
                return *current_;
            }

            constexpr CharT* operator->() const noexcept
            {
                return current_;
            }


            friend constexpr std::strong_ordering operator<=>(iterator_type_ const&, iterator_type_ const&) noexcept = default;
        };

        // ********************************* begin iterator function ******************************
    public:
        using iterator = iterator_type_;
        using const_iterator = std::basic_const_iterator<iterator_type_>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr iterator begin() noexcept
        {
#ifndef NDEBUG
            return { begin_(), this };
#else
            return { begin_() };
#endif // _DEBUG
        }

        constexpr iterator end() noexcept
        {
#ifndef NDEBUG
            return iterator_type_{ end_(), this };
#else
            return iterator_type_{ end_() };
#endif // _DEBUG
        }

        constexpr const_iterator begin() const noexcept
        {
#ifndef NDEBUG
            return iterator_type_{ const_cast<CharT*>(begin_()), const_cast<basic_string*>(this) };
#else
            return iterator_type_{ begin_() };
#endif // _DEBUG
        }

        constexpr const_iterator end() const noexcept
        {
#ifndef NDEBUG
            return iterator_type_{ const_cast<CharT*>(end_()), const_cast<basic_string*>(this) };
#else
            return iterator_type_{ const_cast<CharT*>(end_()) };
#endif // _DEBUG
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
         * @brief allocates memory and automatically adds 1 to store trailing zero
         * @brief strong exception safety guarantee
         * @brief not responsible for reclaiming memory
         * @brief never shrink
         * @param n, expected number of characters
         */
        constexpr void allocate_plus_one_(size_type n)
        {
            assert(("string is null, cannot call ", !is_null_()));
            // strong exception safe grantee
            if (n <= short_string_max_ && !is_long_())
            {
                size_flag_ = static_cast<signed char>(n);
                return;
            }

            ++n;
            if constexpr (requires { allocator_.allocate_at_least(n); })
            {
                auto&& [ptr, count] = allocator_.allocate_at_least(n);
                stor_.ls_ = { ptr, nullptr, ptr + count };
            }
            else
            {
                auto&& ptr = allocator_.allocate(n);
                stor_.ls_ = { ptr, nullptr, ptr + n };
            }
            size_flag_ = -1;
        }

        /**
         * @brief dealloc the memory of long string
         * @brief static member function
         * @param ls, allocated long string
         */
        constexpr void dealloc_(ls_type_& ls) noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            allocator_.deallocate(ls.begin_, ls.last_ - ls.begin_);
        }

        /**
         * @brief conditionally sets size correctly. only legal if n < capacity()
         * @brief write 0 to the tail at the same time
         * @brief never shrink
         * @param n, expected string length
         */
        constexpr void resize_(size_type n) noexcept
        {
            assert(("n > capacity()", n <= capacity()));
            if (is_long_())
            {
                // if n = 0, keep storage avilable
                auto&& ls = stor_.ls_;
                ls.end_ = ls.begin_ + n;
                // return advance
                *ls.end_ = CharT{};
            }
            else
            {
                size_flag_ = static_cast<signed int>(n);
                stor_.ss_[n] = CharT{};
            }
        }

        /**
         * @brief fill characters to *this
         * @brief not doing anything else
         * @param begin begin of characters
         * @param end end of characters
         */
        constexpr void fill_(CharT const* begin, CharT const* end) noexcept
        {
            assert(("string is null, cannot call ", !is_null_()));
            assert(("cannot storage string in current allocated storage", static_cast<size_type>(end - begin) <= capacity()));
            std::copy(begin, end, begin_());
        }

        /**
         * @brief caculate the length of c style string
         * @param begin begin of characters
         * @return length
         */
        constexpr static size_type c_style_string_length_(CharT const* begin) noexcept
        {
            if constexpr (!std::is_constant_evaluated())
            {
                if constexpr (std::is_same_v<char, CharT>)
                {
                    return std::strlen(begin);
                }
                else if (std::is_same_v<wchar_t, CharT>)
                {
                    return std::wcslen(begin);
                }
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
         * @brief this function provide for assign and operator=
         * @param begin begin of characters
         * @param end end of characters
         */
        constexpr void assign_(CharT const* begin, CharT const* end)
        {
            auto size = end - begin;
            if (capacity() < static_cast<size_type>(size))
            {
                if (is_long_())
                {
                    auto ls = stor_.ls_;
                    allocate_plus_one_(size);
                    dealloc_(ls);
                }
                else
                {
                    allocate_plus_one_(size);
                }
            }

            fill_(begin, end);
            resize_(size);
        }

    public:
        /**
         * @brief reserve memory
         * @brief strong exception safety guarantee
         * @brief never shrink
         * @param new_cap new capacity
         */
        constexpr void reserve(size_type new_cap)
        {
            if (capacity() < new_cap)
            {
                if (is_long_())
                {
                    auto ls = stor_.ls_;
                    allocate_plus_one_(new_cap);
                    fill_(ls.begin_, ls.end_);
                    dealloc_(ls);
                    resize_(ls.end_ - ls.begin_);
                }
                else
                {
                    auto ss = stor_.ss_;
                    auto size = size_flag_;
                    auto data = ss.data();
                    allocate_plus_one_(new_cap);
                    fill_(data, data + size);
                    resize(size);
                }
            }
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
            if (count > size_())
            {
                reserve(count);
                auto end = end_();
                std::fill(end, end + count, ch);
            }
            resize_(count);
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
         * @brief equal to resize(0)
         * @brief never shrink
         */
        constexpr void clear() noexcept
        {
            resize_(0);
        }

        /**
         * @brief use size * 2 for growth
         * @param ch character to fill
         */
        constexpr void push_back(CharT ch)
        {
            auto size = size_();
            if (capacity() == size)
            {
                // long string
                if (is_long_())
                {
                    auto ls = stor_.ls_;
                    allocate_plus_one_(size * 2);
                    fill_(ls.begin_, ls.end_);
                    dealloc_(ls);
                    *(ls.end_ - 1) = ch;
                }
                else
                {
                    auto ss = stor_.ss_;
                    auto data = ss.data();
                    allocate_plus_one_(size * 2);
                    fill_(ss, ss + size);
                    *(ss + size) = ch;
                }
            }
            resize_(size + 1);
        }

        // ********************************* begin constructor ******************************
        constexpr basic_string() noexcept = default;

        constexpr basic_string(null_t) noexcept
        {
            size_flag_ = -2;
        }

        constexpr basic_string(size_type n, CharT ch)
        {
            allocate_plus_one_(n);
            for (auto begin = begin_(), end = begin + n; begin != end; ++begin)
            {
                *begin = ch;
            }
            resize_(n);
        }

        constexpr basic_string(const basic_string& other, size_type pos, size_type count) : allocator_(other.allocator_)
        {
            assert(("pos too long", other.size_() > pos));
            auto size = other.size_();
            count = (size < pos + count) ? size - pos : count;
            allocate_plus_one_(count);
            auto&& begin = other.begin_();
            fill_(begin + pos, begin + pos + count);
            resize_(count);
        }

        constexpr basic_string(const basic_string& other, size_type pos) : basic_string(other, pos, other.size_() - (pos + 1))
        {
        }

        constexpr basic_string(basic_string&& other, size_type pos, size_type count)
        {
            assert(("count too long", other.size_() > count));
            if (pos != 0)
            {
                allocate_plus_one_(count);
                for (auto start = other.begin_(), begin = start + pos, end = other.begin_() + pos + count; begin != end; ++begin, ++start)
                {
                    *start = *begin;
                }
                resize_(count);
            }
            else
            {
                other.resize_(count);
                other.swap(*this);
            }
        }

        constexpr basic_string(basic_string&& other, size_type pos) : basic_string(std::move(other), pos, other.size_() - pos)
        {
        }

        constexpr basic_string(const CharT* s, size_type count)
        {
            allocate_plus_one_(count);
            fill_(s, s + count);
            resize_(count);
        }

        constexpr basic_string(const CharT* s) : basic_string(s, c_style_string_length_(s))
        {
        }

        template <class InputIt>
            requires std::input_iterator<InputIt>
        constexpr basic_string(InputIt first, InputIt last)
        {
            if constexpr (std::random_access_iterator<InputIt>)
            {
                auto length = std::ranges::distance(first, last);
                allocate_plus_one_(length);
                std::ranges::copy(first, last, begin_());
                resize_(length);
            }
            else
            {
                for (; first != last; ++first)
                    push_back(*first);
            }
        }

        constexpr basic_string(const basic_string& other, const Allocator& alloc) : allocator_(alloc)
        {
            auto size = other.size_();
            allocate_plus_one_(size);
            fill_(other.begin_(), other.end_());
            resize_(size);
        }

        constexpr basic_string(const basic_string& other) : basic_string(other, other.allocator_)
        {
        }

        constexpr basic_string(basic_string&& other) noexcept
        {
            other.swap(*this);
        }

        constexpr basic_string(basic_string&& other, const Allocator& alloc)
        {
            if (other.allocator_ == alloc)
                other.swap(*this);
            else
            {
                allocator_ = alloc;
                allocate_plus_one_(other.size_());
                fill_(other.begin_(), other.end_());
                resize_(other.size_());
            }
        }

        constexpr basic_string(std::initializer_list<CharT> ilist) : basic_string(std::data(ilist), ilist.size())
        {
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string(const StringViewLike& t) : basic_string(std::basic_string_view<CharT, Traits>{ t }.data(), std::basic_string_view<CharT, Traits>{ t }.size())
		{
		}

        // clang-format on

        template <class StringViewLike>
            requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>>
        constexpr basic_string(const StringViewLike& t, size_type pos, size_type n) : basic_string(assert(("pos + n larger than size", std::basic_string_view<CharT, Traits>{ t }.size() >= pos + n)), std::basic_string_view<CharT, Traits>{ t }.data() + pos, n)
        {
        }

        /*
        template <class R>
            requires std::ranges::range<R> && requires {
                typename R::value_type;
                requires std::same_as<typename R::value_type, CharT>;
            }
        constexpr basic_string(std::from_range_t, R&& rg)
        {
            if constexpr (std::ranges::sized_range<R>)
            {
                auto size = std::ranges::size(rg);
                if constexpr (std::random_access_iterator<typename std::iterator_traits<R>::iterator>)
                {
                    allocate_plus_one_(size);
                    std::ranges::copy(std::ranges::begin(rg), std::ranges::end(rg), begin_());
                    resize_(size);
                }
                else
                {
                    allocate_plus_one_(size);
                    resize_(0);
                    for (auto first = std::ranges::begin(rg), last = std::ranges::end(rg); first != last; ++first)
                        push_back(*first);
                }
            }
            else
            {
                for (auto first = std::ranges::begin(rg), last = std::ranges::end(rg); first != last; ++first)
                    push_back(*first);
            }
        }
        */
        // ********************************* begin append ******************************
        /**
         * @brief this version provide for InputIt version of assign, other version of append and operator+=
         */
        template <class InputIt>
            requires std::input_iterator<InputIt>
        constexpr basic_string& append(InputIt first, InputIt last)
        {
            if constexpr (std::random_access_iterator<InputIt>)
            {
                auto size = size_();
                auto length = std::ranges::distance(first, last);
                auto new_size = size + length;
                reserve(size * 2 > static_cast<size_type>(new_size) ? size * 2 : new_size);
                std::ranges::copy(first, last, begin_() + size);
                resize_(new_size);
            }
            else
            {
                for (; first != last; ++first)
                    push_back(*first);
            }
            return *this;
        }

        // ********************************* begin assign ******************************

        constexpr basic_string& assign(size_type count, CharT ch)
        {
            reserve(count);
            for (auto begin = begin_(), end = begin + count; begin != end; ++begin)
            {
                *begin = ch;
            }
            resize_(count);
        }

        constexpr basic_string& assign(const basic_string& str)
        {
            assign_(str.begin_(), str.end_());
            return *this;
        }

        constexpr basic_string& assign(const basic_string& str, size_type pos, size_type count = npos)
        {
            auto size = str.size_();
            assign_(str.begin_() + pos, str.begin_() + pos + count);
            return *this;
        }

        constexpr basic_string& assign(basic_string&& str) noexcept
        {
            str.swap(*this);
            return *this;
        }

        constexpr basic_string& assign(const CharT* s, size_type count)
        {
            assign_(s, s + count);
            return *this;
        }

        constexpr basic_string& assign(const CharT* s)
        {
            assign_(s, s + c_style_string_length_(s));
            return *this;
        }

        template <class InputIt>
            requires std::input_iterator<InputIt>
        constexpr basic_string& assign(InputIt first, InputIt last)
        {
            resize_(0);
            return append(first, last);
        }

        constexpr basic_string& assign(std::initializer_list<CharT> ilist)
        {
            auto data = std::data(ilist);
            assign_(data, data + ilist.size());
            return *this;
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		basic_string& assign(const StringViewLike& t)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			auto data = sv.data();
			assign_(data, data + sv.size());
			return *this;
		}

		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& assign(const StringViewLike& t, size_type pos, size_type count = npos)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			auto size = sv.size();
			if (pos > size)
				throw std::out_of_range{ {} };
			count = (size < pos + count || count == npos) ? size - pos : count;
			auto data = sv.data();
			assign_(data + pos, data + pos + count);
			return *this;
		}

        // clang-format on
        // ********************************* begin operator= ******************************
        basic_string(std::nullptr_t) = delete;
        constexpr basic_string& operator=(std::nullptr_t) = delete;

        constexpr basic_string& operator=(basic_string&& other) noexcept(nothrow_move_allocator_)
        {
            other.swap(*this);
            return *this;
        }

        constexpr basic_string& operator=(const basic_string& str)
        {
            allocator_ = str.allocator_;
            assign_(str.begin_(), str.end_());
            return *this;
        }

        constexpr basic_string& operator=(const CharT* s)
        {
            return assign(s);
        }

        constexpr basic_string& operator=(CharT ch)
        {
            resize_(1);
            (*begin_()) = ch;
            return *this;
        }

        constexpr basic_string& operator=(std::initializer_list<CharT> ilist)
        {
            return assign(ilist);
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& operator=(const StringViewLike& t)
		{
			return assign(t);
		}

        // clang-format on

        // ********************************* begin compare ******************************
        friend constexpr std::strong_ordering operator<=>(basic_string const& lhs, basic_string const& rhs)
        {
            if (lhs.size_() > rhs.size_())
                return std::strong_ordering::greater;
            else if (lhs.size_() < rhs.size_())
                return std::strong_ordering::less;
            for (auto begin = lhs.begin_(), end = lhs.end_(), start = rhs.begin_(); begin != end; ++begin, ++start)
            {
                if (*begin > *start)
                    return std::strong_ordering::greater;
                else if (*begin < *start)
                    return std::strong_ordering::less;
            }
            return std::strong_ordering::equal;
        }

        friend constexpr bool operator==(basic_string const& lhs, basic_string const& rhs)
        {
            if (lhs.size_() != rhs.size_())
                return false;
            for (auto begin = lhs.begin_(), end = lhs.end_(), start = rhs.begin_(); begin != end; ++begin, ++start)
            {
                if (*begin != *start)
                    return false;
            }
            return true;
        }

        // ********************************* begin append ******************************

        constexpr basic_string& append(const CharT* s, size_type count)
        {
            return append(s, s + count);
        }

        constexpr basic_string& append(const basic_string& str)
        {
            return append(str.data(), str.size_());
        }

        constexpr basic_string& append(const basic_string& str, size_type pos, size_type count = npos)
        {
            auto size = str.size_();
            if (pos > size)
                throw std::out_of_range{ {} };
            count = (size < pos + count || count == npos) ? size - pos : count;
            return append(str.begin_() + pos, count);
        }

        constexpr basic_string& append(const CharT* s)
        {
            return append(s, c_style_string_length_(s));
        }

        constexpr basic_string& append(std::initializer_list<CharT> ilist)
        {
            return append(std::data(ilist), ilist.size());
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& append(const StringViewLike& t)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			return append(sv.data(), sv.size());
		}
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& append(const StringViewLike& t, size_type pos, size_type count = npos)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			auto size = sv.size();
			if (pos > size)
				throw std::out_of_range{ {} };
			count = (size < pos + count || count == npos) ? size - pos : count;
			return append(sv.data() + pos, count);
		}

        // clang-format on
        // ********************************* begin operator+= ******************************
        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& operator+=(const StringViewLike& t)
		{
			return append(t);
		}

        // clang-format on
        constexpr basic_string& operator+=(const basic_string& str)
        {
            return append(str.begin_(), str.end_());
        }

        constexpr basic_string& operator+=(CharT ch)
        {
            push_back(ch);
            return *this;
        }

        constexpr basic_string& operator+=(const CharT* s)
        {
            return append(s, c_style_string_length_(s));
        }

        constexpr basic_string& operator+=(std::initializer_list<CharT> ilist)
        {
            auto data = std::data(ilist);
            return append(data, ilist.size());
        }

        // ********************************* begin search ******************************
    private:
        constexpr bool static equal_(CharT const* begin, CharT const* end, CharT const* first, CharT const* last) noexcept
        {
            if (last - first != end - begin)
                return false;
            for (; begin != end; ++begin, ++first)
            {
                if (*first != *begin)
                {
                    return false;
                }
            }
            return true;
        }

    public:
        constexpr bool starts_with(std::basic_string_view<CharT, Traits> sv) const noexcept
        {
            auto size = sv.size();
            auto data = sv.data();
            auto begin = begin_();
            if (size > size_())
                return false;
            return equal_(data, data + size, begin, begin + size);
        }

        constexpr bool starts_with(CharT ch) const noexcept
        {
            if (empty() || *begin_() != ch)
                return false;
            return true;
        }

        constexpr bool starts_with(const CharT* s) const
        {
            auto length = c_style_string_length_(s);
            auto begin = begin_();
            if (length > size_())
                return false;
            return equal_(s, s + length, begin, begin + length);
        }

        constexpr bool ends_with(std::basic_string_view<CharT, Traits> sv) const noexcept
        {
            auto size = sv.size();
            auto data = sv.data();
            auto end = end_();
            if (size > size_())
                return false;
            return equal_(data, data + size, end - size, end);
        }

        constexpr bool ends_with(CharT ch) const noexcept
        {
            if (empty() || *end_() != ch)
                return false;
            return true;
        }

        constexpr bool ends_with(CharT const* s) const
        {
            auto length = c_style_string_length_(s);
            auto end = end_();
            if (length > size_())
                return false;
            return equal_(s, s + length, end - length, end);
        }

        // ********************************* begin destructor ******************************
        constexpr ~basic_string()
        {
            if (is_long_())
            {
                dealloc_(stor_.ls_);
            }
        }
    };
}