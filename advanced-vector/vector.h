#pragma once
#include <algorithm>
#include <cassert>
#include <memory>
#include <cstdlib>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index <= capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_),
        size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (size_ == rhs.size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_ + size_, data_.GetAddress());
                    return *this;
                }
                size_t size_for_copy = std::min(size_, rhs.size_);
                std::copy(rhs.data_.GetAddress(), rhs.data_ + size_for_copy, data_.GetAddress());
                if (size_ > rhs.size_) {
                    std::destroy_n(data_ + size_for_copy, size_ - rhs.size_);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_ + size_for_copy, rhs.size_ - size_, data_ + size_for_copy);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index <= size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        SwapData(new_data);
    }

    void Resize(size_t new_size) {
        if (size_ == new_size) {
            return;
        }
        if (size_ > new_size) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(value);
            SwapData(new_data);
        }
        else {
            new (data_ + size_) T(value);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::move(value));
            SwapData(new_data);
        }
        else {
            new (data_ + size_) T(std::move(value));
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            SwapData(new_data);
        }
        else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    void PopBack() /* noexcept */ {
        std::destroy_at(data_ + (size_ - 1));
        --size_;
    }

    ~Vector() {
        if (size_ > 0) {
            std::destroy_n(data_.GetAddress(), size_);
        }
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == cend()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }
        size_t new_element_pos = pos - cbegin();
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + new_element_pos) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), new_element_pos, new_data.GetAddress());
                std::uninitialized_move_n(data_ + new_element_pos, size_ - new_element_pos, new_data + new_element_pos + 1);
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), new_element_pos, new_data.GetAddress());
                std::uninitialized_copy_n(data_ + new_element_pos, size_ - new_element_pos, new_data + new_element_pos + 1);
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

        }
        else {
            T tmp(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(*std::prev(end())));
            std::move_backward(data_ + new_element_pos, end() - 1, end());
            data_[new_element_pos] = std::move(tmp);
        }
        ++size_;
        return &data_[new_element_pos];
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        assert(pos >= begin() && pos < end());
        size_t element_to_erase_index = pos - cbegin();
        iterator element_to_erase_pos = data_ + element_to_erase_index;
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::move(element_to_erase_pos + 1, end(), element_to_erase_pos);
        }
        else {
            std::copy(element_to_erase_pos + 1, end(), element_to_erase_pos);
        }
        std::destroy_at(std::prev(end()));
        --size_;
        return element_to_erase_pos;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:

    void SwapData(RawMemory<T>& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
