#ifndef S2_S2CELL_ID_H_
#define S2_S2CELL_ID_H_

#include "s2/s2cell_id_v1.h"
#include "s2/s2latlng.h"
#include "s2/s2point.h"
#include "s2/r2.h"       // 为R2Point
#include "s2/r2rect.h"   // 为R2Rect
#include "s2/s1angle.h"  // 为S1Angle
#include "s2/s2error.h"  // 为S2Error
#include "s2/s2coder.h"  // 为s2coding命名空间
#include "s2/util/coding/coder.h" // 为Encoder/Decoder
#include "absl/strings/string_view.h"  // 为absl::string_view
#include "absl/hash/hash.h"            // 为absl::Hash
#include "absl/numeric/bits.h"         // 为absl::rotr
#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <exception>
#include <algorithm>     // 为std::min/max

// 前向声明，避免命名冲突
using OriginalS2CellId = s2v1::S2CellId;

// Import standard types
using std::uint64_t;

// S2CellId的新编码格式实现
// 编码格式: 3位face + k个子节点编码(每个2位) + 若干0 + 最后5位表示层数
// 最大支持28层
class S2CellId {
public:
    static constexpr int kFaceBits = 3;
    static constexpr int kNumFaces = 6;
    static constexpr int kLevelBits = 5;
    static constexpr int kMaxLevel = 28; // 限制到28层
    static constexpr int kPosBits = 2 * kMaxLevel + 1; // 保持兼容性
    static constexpr int kMaxSize = 1 << kMaxLevel; // 叶子层级的最大尺寸
    static constexpr int kPathBits = 64 - kFaceBits - kLevelBits; // 56位
    static constexpr uint64_t kFaceMask = (1ULL << kFaceBits) - 1;
    static constexpr uint64_t kLevelMask = (1ULL << kLevelBits) - 1;
    
    // 默认构造函数
    S2CellId() = default;
    
    // 创建无效的S2CellId
    static constexpr S2CellId None() { return S2CellId(); }
    
    // 从新编码构造
    explicit constexpr S2CellId(uint64_t new_id) : new_id_(new_id) {}
    
    // 从原OriginalS2CellId构造
    explicit S2CellId(const OriginalS2CellId& old_id) {
        new_id_ = ConvertFromOldFormat(old_id);
    }
    
    // 从S2Point构造
    explicit S2CellId(const S2Point& point) {
        OriginalS2CellId old_id(point);
        if (old_id.level() > kMaxLevel) {
            old_id = old_id.parent(kMaxLevel);
        }
        new_id_ = ConvertFromOldFormat(old_id);
    }
    
    // 从S2LatLng构造
    explicit S2CellId(const S2LatLng& latlng) {
        OriginalS2CellId old_id(latlng);
        if (old_id.level() > kMaxLevel) {
            old_id = old_id.parent(kMaxLevel);
        }
        new_id_ = ConvertFromOldFormat(old_id);
    }
    
    // 从OriginalS2CellId构造（推荐使用的方法名）
    static S2CellId FromS2CellId(const OriginalS2CellId& old_id) {
        return S2CellId(old_id);
    }
    
    // 检查是否可以用新格式完全表示
    static bool CanRepresentInNewFormat(const OriginalS2CellId& old_id) {
        return old_id.level() <= kMaxLevel;
    }
    
    // 从face和level构造
    static S2CellId FromFaceLevel(int face, int level) {
        if (face < 0 || face > 5 || level < 0 || level > kMaxLevel) {
            return S2CellId(); // 返回无效ID
        }
        
        if (level == 0) {
            // 根节点，直接构造
            uint64_t id = (static_cast<uint64_t>(face) << (64 - kFaceBits)) | 
                          static_cast<uint64_t>(level);
            // 对于face=0, level=0的情况，new_id会是0，这会被is_valid()判断为无效
            // 我们需要确保有效的cell都有非零的new_id
            if (id == 0) {
                // 设置一个特殊标记位，表示这是有效的face=0根节点
                id = 1ULL << (kLevelBits + kPathBits - 1); // 设置path的最高位
            }
            return S2CellId(id);
        } else {
            // 非根节点，使用FromFacePosLevel创建第一个子cell
            OriginalS2CellId old_id = OriginalS2CellId::FromFacePosLevel(face, 0, level);
            if (!old_id.is_valid()) {
                return S2CellId(); // 返回无效ID
            }
            return S2CellId(old_id);
        }
    }
    
    // 从点构造
    static S2CellId FromPoint(const S2Point& point) {
        OriginalS2CellId old_id(point);
        // 如果原始ID的层级超过支持范围，选择一个合适的层级
        if (old_id.level() > kMaxLevel) {
            // 获取指定层级的祖先cell
            OriginalS2CellId ancestor = old_id.parent(kMaxLevel);
            return S2CellId(ancestor);
        }
        return S2CellId(old_id);
    }
    
    // 从经纬度构造
    static S2CellId FromLatLng(const S2LatLng& latlng) {
        OriginalS2CellId old_id(latlng);
        // 如果原始ID的层级超过支持范围，选择一个合适的层级
        if (old_id.level() > kMaxLevel) {
            // 获取指定层级的祖先cell
            OriginalS2CellId ancestor = old_id.parent(kMaxLevel);
            return S2CellId(ancestor);
        }
        return S2CellId(old_id);
    }
    
    // ==================== 静态工厂方法（委托实现） ====================
    
    static S2CellId FromFace(int face) {
        OriginalS2CellId old_id = OriginalS2CellId::FromFace(face);
        return S2CellId(old_id);
    }
    
    static S2CellId FromFacePosLevel(int face, uint64_t pos, int level) {
        if (level > kMaxLevel) return S2CellId(); // 超出支持范围
        
        OriginalS2CellId old_id = OriginalS2CellId::FromFacePosLevel(face, pos, level);
        return S2CellId(old_id);
    }
    
    static S2CellId FromFaceIJ(int face, int i, int j) {
        OriginalS2CellId old_id = OriginalS2CellId::FromFaceIJ(face, i, j);
        if (old_id.level() > kMaxLevel) {
            // 如果层级太高，选择合适的祖先
            old_id = old_id.parent(kMaxLevel);
        }
        return S2CellId(old_id);
    }
    
    static S2CellId FromToken(absl::string_view token) {
        OriginalS2CellId old_id = OriginalS2CellId::FromToken(token);
        if (!old_id.is_valid()) return S2CellId();
        
        if (old_id.level() > kMaxLevel) {
            old_id = old_id.parent(kMaxLevel);
        }
        return S2CellId(old_id);
    }
    
    static S2CellId Begin(int level) {
        if (level > kMaxLevel) return S2CellId();
        
        OriginalS2CellId old_id = OriginalS2CellId::Begin(level);
        return S2CellId(old_id);
    }
    
    static S2CellId End(int level) {
        if (level > kMaxLevel) return S2CellId();
        
        OriginalS2CellId old_id = OriginalS2CellId::End(level);
        return S2CellId(old_id);
    }
    
    static constexpr S2CellId Sentinel() {
        return S2CellId(~uint64_t{0});
    }
    
    // 获取新编码ID
    uint64_t new_id() const { return new_id_; }
    
    // 转换为原OriginalS2CellId格式
    OriginalS2CellId ToOldFormat() const {
        return ConvertToOldFormat(new_id_);
    }
    
    // 转换为OriginalS2CellId（推荐使用的方法名）
    OriginalS2CellId ToS2CellId() const {
        return ConvertToOldFormat(new_id_);
    }
    
    // ==================== 位置和几何属性（委托实现） ====================
    
    uint64_t id() const {
        return ToOldFormat().id();
    }
    
    // 获取新格式的ID（避免混淆）
    uint64_t id_v2() const {
        return new_id_;
    }
    
    uint64_t pos() const {
        return ToOldFormat().pos();
    }
    
    int GetSizeIJ() const {
        return ToOldFormat().GetSizeIJ();
    }
    
    static int GetSizeIJ(int level) {
        return OriginalS2CellId::GetSizeIJ(level);
    }
    
    double GetSizeST() const {
        return ToOldFormat().GetSizeST();
    }
    
    static double GetSizeST(int level) {
        return OriginalS2CellId::GetSizeST(level);
    }
    
    // 基本属性获取
    int face() const {
        // 特殊处理：对于face=0, level=0的根节点，我们使用了特殊标记位
        if (new_id_ == (1ULL << (kLevelBits + kPathBits - 1))) {
            return 0;
        }
        return static_cast<int>(new_id_ >> (64 - kFaceBits));
    }
    
    int level() const {
        // 特殊处理：对于face=0, level=0的根节点，我们使用了特殊标记位
        if (new_id_ == (1ULL << (kLevelBits + kPathBits - 1))) {
            return 0;
        }
        return static_cast<int>(new_id_ & kLevelMask);
    }
    
    uint64_t path() const {
        int l = level();
        if (l == 0) return 0; // 根节点没有路径
        
        uint64_t mask = (1ULL << kPathBits) - 1;
        uint64_t raw_path = (new_id_ >> kLevelBits) & mask;
        
        // 只返回有效的路径位，避免包含无关的高位
        if (l > 0 && l <= kMaxLevel) { // 确保level在有效范围内
            int shift_bits = l * 2;
            // 由于kMaxLevel=28，shift_bits最大为56，所以移位是安全的
            uint64_t valid_path_mask = (1ULL << shift_bits) - 1;
            return raw_path & valid_path_mask;
        }
        
        return raw_path;
    }
    
    // 检查有效性
    bool is_valid() const {
        if (new_id_ == 0) return false;
        
        int f = static_cast<int>(new_id_ >> (64 - kFaceBits));
        int l = static_cast<int>(new_id_ & kLevelMask);
        
        // 特殊处理：对于face=0, level=0的根节点，我们使用了特殊标记位
        // 检查是否是这种特殊情况
        if (l == 0 && f == 0 && new_id_ == (1ULL << (kLevelBits + kPathBits - 1))) {
            return true;
        }
        
        // 快速检查face和level范围
        if (f < 0 || f >= 6) return false;
        if (l < 0 || l > kMaxLevel) return false;
        
        // 对于根节点，不需要检查路径
        if (l == 0) return true;
        
        // 检查路径的有效性
        uint64_t mask = (1ULL << kPathBits) - 1;
        uint64_t current_path = (new_id_ >> kLevelBits) & mask;
        
        // 检查路径是否符合层级要求 - 路径的高位应该为0
        if (l > 0) { // 对于非根节点，需要检查路径有效性
            // 路径应该只使用l*2位，高位应该为0
            int used_bits = l * 2;
            if (used_bits < kPathBits) {
                uint64_t high_bits_mask = ~((1ULL << used_bits) - 1);
                if ((current_path & high_bits_mask) != 0) return false;
            }
        }
        
        return true;
    }
    
    // 层级关系操作
    S2CellId parent() const {
        if (!is_valid()) return S2CellId();
        
        if (level() == 0) return S2CellId(); // 根节点没有父节点
        
        int parent_level = level() - 1;
        uint64_t parent_path = path() >> 2;
        
        uint64_t parent_id = (static_cast<uint64_t>(face()) << (64 - kFaceBits)) |
                            (parent_path << kLevelBits) |
                            static_cast<uint64_t>(parent_level);
        
        // 特殊处理：对于face=0, level=0的根节点，需要设置特殊标记
        if (parent_id == 0) {
            parent_id = 1ULL << (kLevelBits + kPathBits - 1);
        }
        
        return S2CellId(parent_id);
    }
    
    // 获取指定层级的父节点
    S2CellId parent(int target_level) const {
        if (target_level < 0 || target_level > kMaxLevel) return S2CellId();
        if (target_level >= level()) return *this;
        
        // 对于大跨度的层级跳跃，使用委托实现更安全
        if (level() - target_level > 5) {
            OriginalS2CellId old_format = ToOldFormat();
            OriginalS2CellId parent_old = old_format.parent(target_level);
            return S2CellId(parent_old);
        }
        
        // 对于小跨度，使用直接实现
        S2CellId current = *this;
        while (current.level() > target_level) {
            current = current.parent();
        }
        return current;
    }
    
    S2CellId child(int position) const {
        if (!is_valid()) return S2CellId();
        if (position < 0 || position >= 4) return S2CellId(); // 无效位置
        
        if (level() >= kMaxLevel) return S2CellId(); // 已经是最大层级
        
        int child_level = level() + 1;
        uint64_t child_path = (path() << 2) | static_cast<uint64_t>(position);
        
        uint64_t child_id = (static_cast<uint64_t>(face()) << (64 - kFaceBits)) |
                           (child_path << kLevelBits) |
                           static_cast<uint64_t>(child_level);
        
        return S2CellId(child_id);
    }
    
    // ==================== 坐标转换（委托实现） ====================
    
    S2Point ToPointRaw() const {
        return ToOldFormat().ToPointRaw();
    }
    
    R2Point GetCenterST() const {
        return ToOldFormat().GetCenterST();
    }
    
    R2Point GetCenterUV() const {
        return ToOldFormat().GetCenterUV();
    }
    
    R2Rect GetBoundST() const {
        return ToOldFormat().GetBoundST();
    }
    
    R2Rect GetBoundUV() const {
        return ToOldFormat().GetBoundUV();
    }
    
    int ToFaceIJOrientation(int* pi, int* pj, int* orientation) const {
        return ToOldFormat().ToFaceIJOrientation(pi, pj, orientation);
    }
    
    int GetCenterSiTi(int* psi, int* pti) const {
        return ToOldFormat().GetCenterSiTi(psi, pti);
    }
    
    // ==================== 层级关系（部分委托实现） ====================
    
    // ==================== 范围操作（委托实现） ====================
    
    S2CellId range_min() const {
        if (!is_valid()) return S2CellId(); // Invalid input returns invalid output
        
        OriginalS2CellId old_format = ToOldFormat();
        if (!old_format.is_valid()) return S2CellId(); // Failed conversion
        
        OriginalS2CellId range_min_old = old_format.range_min();
        if (!range_min_old.is_valid()) return S2CellId(); // Invalid range_min
        
        // Check if the range_min level exceeds our limit
        if (range_min_old.level() > kMaxLevel) {
            range_min_old = range_min_old.parent(kMaxLevel);
        }
        
        return S2CellId(range_min_old);
    }
    
    S2CellId range_max() const {
        if (!is_valid()) return S2CellId(); // Invalid input returns invalid output
        
        OriginalS2CellId old_format = ToOldFormat();
        if (!old_format.is_valid()) return S2CellId(); // Failed conversion
        
        OriginalS2CellId range_max_old = old_format.range_max();
        if (!range_max_old.is_valid()) return S2CellId(); // Invalid range_max
        
        // Check if the range_max level exceeds our limit
        if (range_max_old.level() > kMaxLevel) {
            range_max_old = range_max_old.parent(kMaxLevel);
        }
        
        return S2CellId(range_max_old);
    }
    
    S2CellId maximum_tile(const S2CellId& limit) const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId limit_old = limit.ToOldFormat();
        OriginalS2CellId result_old = old_format.maximum_tile(limit_old);
        
        if (result_old.level() > kMaxLevel) {
            return S2CellId();
        }
        return S2CellId(result_old);
    }
    
    // ==================== 遍历方法（委托实现） ====================
    
    S2CellId child_begin() const {
        if (level() >= kMaxLevel) return S2CellId();
        return child(0);
    }
    
    S2CellId child_begin(int target_level) const {
        if (target_level > kMaxLevel || target_level <= level()) return S2CellId();
        
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId child_old = old_format.child_begin(target_level);
        return S2CellId(child_old);
    }
    
    S2CellId child_end() const {
        if (level() >= kMaxLevel) return S2CellId();
        
        // 使用委托实现来确保正确性
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId child_end_old = old_format.child_end();
        
        if (child_end_old.level() > kMaxLevel) {
            return S2CellId();
        }
        
        return S2CellId(child_end_old);
    }
    
    S2CellId child_end(int target_level) const {
        if (target_level > kMaxLevel || target_level <= level()) return S2CellId();
        
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId child_old = old_format.child_end(target_level);
        return S2CellId(child_old);
    }
    
    // ==================== 导航方法（委托实现） ====================
    
    S2CellId next() const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId next_old = old_format.next();
        
        // 检查结果是否在支持范围内
        if (next_old.level() > kMaxLevel) {
            return S2CellId(); // 超出支持范围
        }
        
        return S2CellId(next_old);
    }
    
    S2CellId prev() const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId prev_old = old_format.prev();
        
        if (prev_old.level() > kMaxLevel) {
            return S2CellId();
        }
        
        return S2CellId(prev_old);
    }
    
    S2CellId next_wrap() const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId next_old = old_format.next_wrap();
        
        if (next_old.level() > kMaxLevel) {
            return S2CellId();
        }
        
        return S2CellId(next_old);
    }
    
    S2CellId prev_wrap() const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId prev_old = old_format.prev_wrap();
        
        if (prev_old.level() > kMaxLevel) {
            return S2CellId();
        }
        
        return S2CellId(prev_old);
    }
    
    S2CellId advance(int64_t steps) const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId result_old = old_format.advance(steps);
        
        if (result_old.level() > kMaxLevel) {
            return S2CellId();
        }
        
        return S2CellId(result_old);
    }
    
    S2CellId advance_wrap(int64_t steps) const {
        OriginalS2CellId old_format = ToOldFormat();
        OriginalS2CellId result_old = old_format.advance_wrap(steps);
        
        if (result_old.level() > kMaxLevel) {
            return S2CellId();
        }
        
        return S2CellId(result_old);
    }
    
    // ==================== 层级关系分析（委托实现） ====================
    
    int GetCommonAncestorLevel(const S2CellId& other) const {
        OriginalS2CellId old1 = ToOldFormat();
        OriginalS2CellId old2 = other.ToOldFormat();
        return old1.GetCommonAncestorLevel(old2);
    }
    
    int64_t distance_from_begin() const {
        return ToOldFormat().distance_from_begin();
    }
    
    // ==================== 邻居查找（委托实现，需要特殊处理） ====================
    
    void GetEdgeNeighbors(S2CellId neighbors[4]) const {
        if (!neighbors) return; // 空指针检查
        
        OriginalS2CellId old_neighbors[4];
        ToOldFormat().GetEdgeNeighbors(old_neighbors);
        
        for (int i = 0; i < 4; ++i) {
            if (old_neighbors[i].is_valid() && old_neighbors[i].level() <= kMaxLevel) {
                neighbors[i] = S2CellId(old_neighbors[i]);
            } else if (old_neighbors[i].is_valid()) {
                // 如果邻居层级太高，选择合适的祖先
                neighbors[i] = S2CellId(old_neighbors[i].parent(kMaxLevel));
            } else {
                neighbors[i] = S2CellId(); // 无效邻居
            }
        }
    }
    
    void AppendVertexNeighbors(int nbr_level, std::vector<S2CellId>* output) const {
        if (!output) return; // 空指针检查
        if (nbr_level > kMaxLevel || nbr_level < 0) return; // 超出支持范围
        
        std::vector<OriginalS2CellId> old_neighbors;
        ToOldFormat().AppendVertexNeighbors(nbr_level, &old_neighbors);
        
        output->reserve(output->size() + old_neighbors.size());
        for (const auto& neighbor : old_neighbors) {
            if (neighbor.is_valid() && neighbor.level() <= kMaxLevel) {
                output->push_back(S2CellId(neighbor));
            }
        }
    }
    
    void AppendAllNeighbors(int nbr_level, std::vector<S2CellId>* output) const {
        if (!output) return; // 空指针检查
        if (nbr_level > kMaxLevel || nbr_level < 0) return;
        
        std::vector<OriginalS2CellId> old_neighbors;
        ToOldFormat().AppendAllNeighbors(nbr_level, &old_neighbors);
        
        output->reserve(output->size() + old_neighbors.size());
        for (const auto& neighbor : old_neighbors) {
            if (neighbor.is_valid() && neighbor.level() <= kMaxLevel) {
                output->push_back(S2CellId(neighbor));
            }
        }
    }
    
    // 委托给原格式的操作
    S2Point ToPoint() const {
        return ToOldFormat().ToPoint();
    }
    
    S2LatLng ToLatLng() const {
        return ToOldFormat().ToLatLng();
    }
    
    bool contains(const S2CellId& other) const {
        return ToOldFormat().contains(other.ToOldFormat());
    }
    
    bool intersects(const S2CellId& other) const {
        return ToOldFormat().intersects(other.ToOldFormat());
    }
    
    // ==================== 编码相关（委托实现） ====================
    
    std::string ToToken() const {
        return ToOldFormat().ToToken();
    }
    
    std::string ToDebugString() const {
        return ToOldFormat().ToString();
    }
    
    // 便利方法
    bool is_leaf() const {
        return level() == kMaxLevel;
    }
    
    bool is_face() const {
        return level() == 0;
    }
    
    // 获取子节点位置
    int child_position() const {
        if (!is_valid()) return -1;
        
        int l = level();
        if (l == 0) return -1; // 根节点没有子节点位置
        
        // 返回最低层级的子节点位置（路径的最后2位）
        return static_cast<int>(path() & 3);
    }
    
    // 获取指定层级的子节点位置
    int child_position(int target_level) const {
        if (!is_valid()) return -1;
        
        int l = level();
        if (target_level <= 0 || target_level > l) return -1;
        
        // 从路径中提取指定层级的子节点位置
        uint64_t current_path = path();
        int shift = 2 * (l - target_level);
        return static_cast<int>((current_path >> shift) & 3);
    }
    
    // 格式化输出
    std::string ToString() const {
        if (!is_valid()) return "INVALID";
        
        int f = face();
        int l = level();
        
        std::string result = std::to_string(f);
        
        if (l == 0) return result; // 根节点只返回face
        
        result += "/";
        
        // 输出子节点路径
        uint64_t current_path = path();
        result.reserve(result.length() + l); // 预分配空间
        
        for (int i = l - 1; i >= 0; --i) {
            int child_pos = static_cast<int>((current_path >> (2 * i)) & 3);
            result += static_cast<char>('0' + child_pos); // 直接转换为字符
        }
        
        return result;
    }
    
    // 从字符串解析
    static S2CellId FromString(const std::string& str) {
        if (str.empty()) return S2CellId();
        
        // 解析格式: "face/childpath" 或 "face/" 或 "face"
        size_t slash_pos = str.find('/');
        
        // 提取face部分
        std::string face_str = (slash_pos == std::string::npos) ? str : str.substr(0, slash_pos);
        
        try {
            int face = std::stoi(face_str);
            if (face < 0 || face > 5) return S2CellId();
            
            // 如果没有斜杠，返回face根节点
            if (slash_pos == std::string::npos) {
                return FromFaceLevel(face, 0);
            }
            
            std::string path_str = str.substr(slash_pos + 1);
            int level = static_cast<int>(path_str.length());
            
            // 处理 "face/" 的情况
            if (level == 0) {
                return FromFaceLevel(face, 0);
            }
            
            // 检查level范围，避免溢出
            if (level > kMaxLevel) return S2CellId();
            
            uint64_t path = 0;
            for (int i = 0; i < level; ++i) {
                char c = path_str[i];
                if (c < '0' || c > '3') return S2CellId();
                
                path = (path << 2) | static_cast<uint64_t>(c - '0');
            }
            
            uint64_t id = (static_cast<uint64_t>(face) << (64 - kFaceBits)) |
                          (path << kLevelBits) |
                          static_cast<uint64_t>(level);
            
            return S2CellId(id);
            
        } catch (const std::exception&) {
            return S2CellId(); // 解析失败返回无效ID
        } catch (...) {
            return S2CellId(); // 捕获所有异常
        }
    }
    
    // 比较操作符
    bool operator==(const S2CellId& other) const {
        return new_id_ == other.new_id_;
    }
    
    bool operator!=(const S2CellId& other) const {
        return new_id_ != other.new_id_;
    }
    
    bool operator<(const S2CellId& other) const {
        // For proper S2 ordering, delegate to the original S2CellId comparison
        return ToOldFormat() < other.ToOldFormat();
    }
    
    // ==================== 其他比较操作符 ====================
    
    bool operator>(const S2CellId& other) const {
        return other < *this;
    }
    
    bool operator<=(const S2CellId& other) const {
        return !(other < *this);
    }
    
    bool operator>=(const S2CellId& other) const {
        return !(*this < other);
    }
    
    // ==================== 便捷的类型转换 ====================
    
    // 隐式转换到OriginalS2CellId（谨慎使用）
    operator OriginalS2CellId() const {
        return ToOldFormat();
    }
    
    // 从OriginalS2CellId赋值
    S2CellId& operator=(const OriginalS2CellId& old_id) {
        new_id_ = ConvertFromOldFormat(old_id);
        return *this;
    }
    
    // 安全的自赋值
    S2CellId& operator=(const S2CellId& other) {
        if (this != &other) {
            new_id_ = other.new_id_;
        }
        return *this;
    }
    
    // ==================== 编码/解码方法 ====================
    
    // 编码方法（委托给S2CellId）
    void Encode(Encoder* encoder) const {
        ToOldFormat().Encode(encoder);
    }
    
    // 解码方法（委托给OriginalS2CellId）
    bool Decode(Decoder* decoder) {
        OriginalS2CellId old_id;
        if (old_id.Decode(decoder)) {
            if (old_id.level() > kMaxLevel) {
                old_id = old_id.parent(kMaxLevel);
            }
            new_id_ = ConvertFromOldFormat(old_id);
            return true;
        }
        return false;
    }
    
    // ==================== 几何扩展方法 ====================
    
    // 扩展边界方法（静态方法，委托给OriginalS2CellId）
    static R2Rect ExpandedByDistanceUV(const R2Rect& uv, S1Angle distance) {
        return OriginalS2CellId::ExpandedByDistanceUV(uv, distance);
    }
    
    // IJLevelToBoundUV 静态方法（委托给OriginalS2CellId）
    static R2Rect IJLevelToBoundUV(int ij[2], int level) {
        return OriginalS2CellId::IJLevelToBoundUV(ij, level);
    }
    
    // ==================== 缺失的低级方法 ====================
    
    // 获取最低有效位
    uint64_t lsb() const {
        return ToOldFormat().lsb();
    }
    
    // 获取指定层级的最低有效位
    static constexpr uint64_t lsb_for_level(int level) {
        return OriginalS2CellId::lsb_for_level(level);
    }
    
    // 从调试字符串构造
    static S2CellId FromDebugString(absl::string_view str) {
        OriginalS2CellId old_id = OriginalS2CellId::FromDebugString(str);
        if (!old_id.is_valid()) return S2CellId();
        
        if (old_id.level() > kMaxLevel) {
            old_id = old_id.parent(kMaxLevel);
        }
        return S2CellId(old_id);
    }
    
    // ==================== Coder类支持 ====================
    
    // Coder类的包装，用于序列化
    class Coder : public s2coding::S2Coder<S2CellId> {
    public:
        void Encode(Encoder& encoder, const S2CellId& v) const override {
            OriginalS2CellId::Coder old_coder;
            old_coder.Encode(encoder, v.ToOldFormat());
        }
        
        bool Decode(Decoder& decoder, S2CellId& v, S2Error& error) const override {
            OriginalS2CellId old_value;
            OriginalS2CellId::Coder old_coder;
            if (old_coder.Decode(decoder, old_value, error)) {
                if (old_value.level() > kMaxLevel) {
                    old_value = old_value.parent(kMaxLevel);
                }
                v = S2CellId(old_value);
                return true;
            }
            return false;
        }
    };
    
private:
    uint64_t new_id_ = 0;
    
    // 转换函数
    static uint64_t ConvertFromOldFormat(const OriginalS2CellId& old_id) {
        if (!old_id.is_valid()) return 0;
        
        int face = old_id.face();
        int level = old_id.level();
        
        // 检查是否超出支持范围
        if (level > kMaxLevel) return 0;
        
        // 验证face范围
        if (face < 0 || face >= 6) return 0;
        
        // 对于根节点，直接构造
        if (level == 0) {
            uint64_t result = (static_cast<uint64_t>(face) << (64 - kFaceBits)) | 0;
            // 对于face=0, level=0的情况，new_id会是0，这会被is_valid()判断为无效
            // 我们需要确保有效的cell都有非零的new_id
            // 解决方案：使用一个"有效位"来标记有效的cell
            // 我们可以使用path的最高位作为有效位
            if (result == 0) {
                // 设置一个特殊标记位，表示这是有效的face=0根节点
                result = 1ULL << (kLevelBits + kPathBits - 1); // 设置path的最高位
            }
            return result;
        }
        
        // 提取子节点路径：从根节点开始，逐级获取子节点位置
        uint64_t path = 0;
        OriginalS2CellId current = old_id;
        
        // 从叶子节点向上追溯到根节点，收集路径
        std::vector<int> path_stack;
        path_stack.reserve(level); // 预分配内存避免重复分配
        
        // 从当前节点向上追溯到根节点
        while (current.level() > 0) {
            int child_pos = current.child_position();
            if (child_pos < 0 || child_pos >= 4) {
                // 无效的子节点位置，返回无效ID
                return 0;
            }
            path_stack.push_back(child_pos);
            current = current.parent();
        }
        
        // 验证最终的父节点是否为正确的face
        if (current.face() != face) {
            return 0; // 路径追溯不一致
        }
        
        // 构建路径（从根到叶子的顺序）
        for (int i = static_cast<int>(path_stack.size()) - 1; i >= 0; --i) {
            path = (path << 2) | static_cast<uint64_t>(path_stack[i]);
        }
        
        // 构建最终的新格式ID
        uint64_t new_id = (static_cast<uint64_t>(face) << (64 - kFaceBits)) |
                          (path << kLevelBits) |
                          static_cast<uint64_t>(level);
        
        return new_id;
    }
    
    static OriginalS2CellId ConvertToOldFormat(uint64_t new_id) {
        if (new_id == 0) return OriginalS2CellId::None();
        
        // 特殊处理：对于face=0, level=0的根节点，我们使用了特殊标记位
        if (new_id == (1ULL << (kLevelBits + kPathBits - 1))) {
            return OriginalS2CellId::FromFace(0);
        }
        
        int face = static_cast<int>(new_id >> (64 - kFaceBits));
        int level = static_cast<int>(new_id & kLevelMask);
        
        // 验证face和level范围
        if (face < 0 || face >= 6) {
            return OriginalS2CellId::None(); // 无效face
        }
        
        if (level < 0 || level > kMaxLevel) {
            return OriginalS2CellId::None(); // 无效level
        }
        
        // 如果是根节点，直接返回
        if (level == 0) {
            return OriginalS2CellId::FromFace(face);
        }
        
        // 从路径重建S2CellId
        OriginalS2CellId result = OriginalS2CellId::FromFace(face);
        uint64_t path = (new_id >> kLevelBits) & ((1ULL << kPathBits) - 1);
        
        for (int i = 0; i < level; ++i) {
            int child_pos = static_cast<int>((path >> (2 * (level - 1 - i))) & 3);
            if (child_pos < 0 || child_pos >= 4) {
                return OriginalS2CellId::None(); // 无效子节点位置
            }
            result = result.child(child_pos);
            if (!result.is_valid()) {
                return OriginalS2CellId::None(); // 子节点操作失败
            }
        }
        
        return result;
    }
};

// Hasher for S2CellId.
// Does *not* need to be specified explicitly; this will be used by default for
// absl::flat_hash_map/set.
//
// TODO(b/259279783): Remove rotation once mixing function on 32-bit systems is
// fixed.
template <typename H>
H AbslHashValue(H h, S2CellId id) {
  if (sizeof(void*) == 4) {
    return H::combine(std::move(h), id.id(), absl::rotr(id.id(), 32));
  }
  return H::combine(std::move(h), id.id());
}

// Legacy hash functor for S2CellId. This only exists for backwards
// compatibility with old hash types like std::unordered_map that don't use
// absl::Hash natively.
struct S2CellIdHash {
  size_t operator()(S2CellId id) const {
    return absl::Hash<S2CellId>()(id);
  }
};

// 输出流操作符
inline std::ostream& operator<<(std::ostream& os, const S2CellId& id) {
    return os << id.ToString();
}

// Parse valid S2 tokens from a string. Returns false if the token cannot be
// parsed with S2CellId::FromToken.
bool AbslParseFlag(absl::string_view input, S2CellId* id, std::string* error);

// Unparse a valid S2 token into a string that can be parsed by AbslParseFlag.
std::string AbslUnparseFlag(S2CellId id);


#endif  // S2_S2CELL_ID_H_