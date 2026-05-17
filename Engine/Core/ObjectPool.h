#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <utility>

namespace Revora {

/// 固定容量のオブジェクトプール (テンプレート、ヘッダオンリー)
///
/// 設計判断:
/// - パーティクルのように頻繁に生成・破棄されるオブジェクトに対して、
///   std::vector の push_back/erase ではなく swap-and-pop 方式で O(1) の
///   取得・返却を実現する
/// - 配列の前半 [0, activeCount) がアクティブ要素、後半が未使用スロット。
///   Release 時に末尾アクティブ要素とスワップして activeCount をデクリメントする
/// - アクティブ要素は連続メモリに配置されるため、
///   イテレーション時のキャッシュ効率が高い
template <typename T>
class ObjectPool {
public:
    /// プールの容量を設定し、要素をデフォルト構築する
    void Initialize(uint32_t capacity)
    {
        elements_.resize(capacity);
        capacity_    = capacity;
        activeCount_ = 0;
    }

    /// プールからオブジェクトを 1 つ取得する
    /// @return 取得したオブジェクトへのポインタ (満杯時は nullptr)
    T* Acquire()
    {
        if (activeCount_ >= capacity_) {
            return nullptr;
        }
        return &elements_[activeCount_++];
    }

    /// 指定インデックスのオブジェクトをプールに返却する
    /// 末尾アクティブ要素とスワップし、activeCount をデクリメントする
    void Release(uint32_t index)
    {
        assert(index < activeCount_ && "Release index out of active range");

        --activeCount_;
        if (index != activeCount_) {
            elements_[index] = std::move(elements_[activeCount_]);
        }
    }

    /// アクティブ要素数を返す
    uint32_t GetActiveCount() const { return activeCount_; }

    /// プール容量を返す
    uint32_t GetCapacity() const { return capacity_; }

    /// アクティブ要素の先頭ポインタ (イテレーション用)
    T* GetActiveElements() { return elements_.data(); }
    const T* GetActiveElements() const { return elements_.data(); }

    /// インデックスでアクティブ要素にアクセスする
    T& operator[](uint32_t index)
    {
        assert(index < activeCount_);
        return elements_[index];
    }
    const T& operator[](uint32_t index) const
    {
        assert(index < activeCount_);
        return elements_[index];
    }

    /// 全アクティブ要素をクリアする
    void Reset()
    {
        activeCount_ = 0;
    }

private:
    std::vector<T> elements_;
    uint32_t capacity_    = 0;
    uint32_t activeCount_ = 0;
};

} // namespace Revora
