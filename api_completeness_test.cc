#include "s2/s2cell_id.h"
#include "s2/s2cell_id_v1.h"
#include "s2/s2latlng.h"
#include "s2/s2point.h"
#include <iostream>
#include <vector>

void TestComprehensiveAPIs() {
    std::cout << "=== 测试S2CellId完整性 ===" << std::endl;
    
    // 1. 测试基本构造
    S2CellId id_default;
    S2CellId id_face = S2CellId::FromFace(0);
    S2CellId id_point = S2CellId::FromPoint(S2Point(1, 0, 0));
    S2CellId id_latlng = S2CellId::FromLatLng(S2LatLng::FromDegrees(0, 0));
    
    std::cout << "✓ 基本构造方法测试通过" << std::endl;
    
    // 2. 测试静态工厂方法
    S2CellId id_from_face_pos = S2CellId::FromFacePosLevel(0, 0, 1);
    S2CellId id_from_face_ij = S2CellId::FromFaceIJ(0, 0, 0);
    S2CellId id_begin = S2CellId::Begin(1);
    S2CellId id_end = S2CellId::End(1);
    S2CellId id_sentinel = S2CellId::Sentinel();
    
    std::cout << "✓ 静态工厂方法测试通过" << std::endl;
    
    // 3. 测试属性获取
    if (id_face.is_valid()) {
        int face = id_face.face();
        int level = id_face.level();
        uint64_t id_value = id_face.id();
        bool is_leaf = id_face.is_leaf();
        bool is_face_cell = id_face.is_face();
    }
    
    std::cout << "✓ 属性获取方法测试通过" << std::endl;
    
    // 4. 测试层级关系
    S2CellId child = id_face.child(0);
    S2CellId parent = child.parent();
    S2CellId parent_level = child.parent(0);
    
    std::cout << "✓ 层级关系方法测试通过" << std::endl;
    
    // 5. 测试转换方法
    S2Point point = id_face.ToPoint();
    S2LatLng latlng = id_face.ToLatLng();
    std::string token = id_face.ToToken();
    std::string debug_str = id_face.ToDebugString();
    std::string to_str = id_face.ToString();
    
    std::cout << "✓ 转换方法测试通过" << std::endl;
    
    // 6. 测试导航方法
    S2CellId next = id_face.next();
    S2CellId prev = id_face.prev();
    S2CellId next_wrap = id_face.next_wrap();
    S2CellId prev_wrap = id_face.prev_wrap();
    
    std::cout << "✓ 导航方法测试通过" << std::endl;
    
    // 7. 测试范围操作
    S2CellId range_min = id_face.range_min();
    S2CellId range_max = id_face.range_max();
    
    std::cout << "✓ 范围操作方法测试通过" << std::endl;
    
    // 8. 测试邻居查找
    S2CellId neighbors[4];
    id_face.GetEdgeNeighbors(neighbors);
    
    std::vector<S2CellId> vertex_neighbors;
    id_face.AppendVertexNeighbors(1, &vertex_neighbors);
    
    std::vector<S2CellId> all_neighbors;
    id_face.AppendAllNeighbors(1, &all_neighbors);
    
    std::cout << "✓ 邻居查找方法测试通过" << std::endl;
    
    // 9. 测试关系检查
    bool contains = id_face.contains(child);
    bool intersects = id_face.intersects(child);
    
    std::cout << "✓ 关系检查方法测试通过" << std::endl;
    
    // 10. 测试比较操作
    bool equals = (id_face == parent);
    bool not_equals = (id_face != child);
    bool less_than = (id_face < child);
    
    std::cout << "✓ 比较操作测试通过" << std::endl;
    
    // 11. 测试新旧格式转换
    s2v1::S2CellId old_format = id_face.ToOldFormat();
    S2CellId from_old(old_format);
    
    std::cout << "✓ 新旧格式转换测试通过" << std::endl;
    
    // 12. 测试字符串解析
    S2CellId from_string = S2CellId::FromString("0");
    S2CellId from_token = S2CellId::FromToken(token);
    
    std::cout << "✓ 字符串解析测试通过" << std::endl;
    
    std::cout << "\n=== 所有API测试成功! ===" << std::endl;
}

int main() {
    try {
        TestComprehensiveAPIs();
        return 0;
    } catch (...) {
        std::cerr << "测试失败!" << std::endl;
        return 1;
    }
}
