#pragma once

#include <cstddef>
#include <cstdint>

namespace Revora {

/// フレーム単位のリニアアロケータ
///
/// 設計判断:
/// - 毎フレームの UI 頂点バッファなど、ライフタイムがフレーム内に閉じる
///   一時データの確保に使用する。Reset() でオフセットを 0 に戻すだけで
///   全確保を一括解放できるため、個別の free が不要で高速。
/// - 汎用アロケータ (malloc/new) と比較して、フラグメンテーションが発生せず
///   キャッシュ局所性も良い。ゲームエンジンでは一般的なパターン
class LinearAllocator {
public:
    LinearAllocator() = default;
    ~LinearAllocator();

    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    /// 指定サイズのバッファを確保する
    /// @param capacity バッファサイズ (バイト)
    void Initialize(std::size_t capacity);

    /// アライメントを考慮してメモリを確保する
    /// @param size 確保サイズ (バイト)
    /// @param alignment アライメント (2の累乗)
    /// @return 確保したメモリへのポインタ (容量不足時は nullptr)
    void* Allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    /// オフセットを先頭に戻す (全確保を一括解放)
    /// フレーム末尾で呼ぶことで、次フレームで同じバッファを再利用する
    void Reset();

    /// 現在の使用量 (バイト)
    std::size_t GetUsedSize() const { return offset_; }

    /// 確保可能な総容量 (バイト)
    std::size_t GetCapacity() const { return capacity_; }

private:
    uint8_t*    buffer_   = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_   = 0;
};

} // namespace Revora
