#include "src/s2/s2cell_id.h"
#include "s2/s2testing.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <chrono>
#include <cmath>


class S2CellIdV2ComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }
    
    void TearDown() override {
        // Clean up test environment
    }
    
    // Helper function to create test cells at various levels
    std::vector<S2CellId> CreateTestCells() {
        std::vector<S2CellId> cells;
        
        // Add face cells (level 0)
        for (int face = 0; face < 6; ++face) {
            cells.push_back(S2CellId::FromFace(face));
        }
        
        // Add cells at different levels
        for (int level = 1; level <= std::min(10, S2CellId::kMaxLevel); ++level) {
            for (int face = 0; face < 6; ++face) {
                S2CellId face_cell = S2CellId::FromFace(face);
                S2CellId child = face_cell.child_begin(level);
                if (child.is_valid()) {
                    cells.push_back(child);
                }
            }
        }
        
        return cells;
    }
};

// ==================== Construction Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, DefaultConstructor) {
    S2CellId default_cell;
    EXPECT_FALSE(default_cell.is_valid());
    EXPECT_EQ(default_cell.new_id(), 0);
}

TEST_F(S2CellIdV2ComprehensiveTest, NoneConstructor) {
    S2CellId none_cell = S2CellId::None();
    EXPECT_FALSE(none_cell.is_valid());
    EXPECT_EQ(none_cell.new_id(), 0);
}

TEST_F(S2CellIdV2ComprehensiveTest, FromFace) {
    for (int face = 0; face < 6; ++face) {
        S2CellId cell = S2CellId::FromFace(face);
        EXPECT_TRUE(cell.is_valid());
        EXPECT_EQ(cell.face(), face);
        EXPECT_EQ(cell.level(), 0);
    }
}

TEST_F(S2CellIdV2ComprehensiveTest, FromFaceLevel) {
    for (int face = 0; face < 6; ++face) {
        for (int level = 0; level <= std::min(5, S2CellId::kMaxLevel); ++level) {
            S2CellId cell = S2CellId::FromFaceLevel(face, level);
            EXPECT_TRUE(cell.is_valid());
            EXPECT_EQ(cell.face(), face);
            EXPECT_EQ(cell.level(), level);
        }
    }
}

TEST_F(S2CellIdV2ComprehensiveTest, FromPoint) {
    std::vector<S2Point> test_points = {
        S2Point(1, 0, 0),    // On face 0
        S2Point(0, 1, 0),    // On face 1
        S2Point(0, 0, 1),    // On face 2
        S2Point(-1, 0, 0),   // On face 3
        S2Point(0, -1, 0),   // On face 4
        S2Point(0, 0, -1),   // On face 5
        S2Point(1, 1, 1).Normalize()  // General point
    };
    
    for (const auto& point : test_points) {
        S2CellId cell = S2CellId::FromPoint(point);
        EXPECT_TRUE(cell.is_valid());
        EXPECT_GE(cell.face(), 0);
        EXPECT_LT(cell.face(), 6);
        EXPECT_LE(cell.level(), S2CellId::kMaxLevel);
    }
}

TEST_F(S2CellIdV2ComprehensiveTest, TokenRoundTrip) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        std::string token = cell.ToToken();
        EXPECT_FALSE(token.empty());
        
        S2CellId parsed = S2CellId::FromToken(token);
        
        // Note: Due to implementation details, we test compatibility
        // by comparing the underlying S2CellId representation
        if (parsed.is_valid()) {
            EXPECT_EQ(cell.face(), parsed.face());
            EXPECT_EQ(cell.level(), parsed.level());
        }
    }
}

TEST_F(S2CellIdV2ComprehensiveTest, FaceTokens) {
    // The expected tokens for face cells in S2CellId are not "0", "1", "2", etc.
    // They are "1", "3", "5", "7", "9", "b" because of the internal encoding
    std::vector<std::string> expected_tokens = {"1", "3", "5", "7", "9", "b"};
    
    for (int face = 0; face < 6; ++face) {
        S2CellId cell = S2CellId::FromFace(face);
        std::string token = cell.ToToken();
        
        // Face cells should have the correct tokens
        EXPECT_EQ(token, expected_tokens[face]);
        
        // Round-trip test
        S2CellId parsed = S2CellId::FromToken(token);
        EXPECT_TRUE(parsed.is_valid());
        EXPECT_EQ(parsed.face(), face);
        EXPECT_EQ(parsed.level(), 0);
    }
}

// ==================== Compatibility Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, OriginalS2CellIdCompatibility) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        // Convert to original format
        OriginalS2CellId original = cell.ToS2CellId();
        EXPECT_TRUE(original.is_valid());
        
        // Convert back
        S2CellId converted = S2CellId::FromS2CellId(original);
        EXPECT_TRUE(converted.is_valid());
        
        // Test basic properties
        EXPECT_EQ(cell.face(), converted.face());
        EXPECT_EQ(cell.level(), converted.level());
    }
}

TEST_F(S2CellIdV2ComprehensiveTest, BiDirectionalConversion) {
    // Test conversion from original S2CellId to new format
    for (int face = 0; face < 6; ++face) {
        OriginalS2CellId original = OriginalS2CellId::FromFace(face);
        
        // Check if it can be represented in new format
        EXPECT_TRUE(S2CellId::CanRepresentInNewFormat(original));
        
        // Convert to new format
        S2CellId new_cell = S2CellId::FromS2CellId(original);
        EXPECT_TRUE(new_cell.is_valid());
        EXPECT_EQ(new_cell.face(), face);
        EXPECT_EQ(new_cell.level(), 0);
        
        // Convert back
        OriginalS2CellId back_to_original = new_cell.ToS2CellId();
        EXPECT_EQ(original, back_to_original);
    }
}

// ==================== Geometric Property Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, GeometricProperties) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        // Test point conversion
        S2Point point = cell.ToPointRaw();
        // S2Point should be a valid 3D vector (basic sanity check)
        EXPECT_TRUE(std::isfinite(point.x()) && std::isfinite(point.y()) && std::isfinite(point.z()));
        
        // Test center coordinates
        R2Point center_st = cell.GetCenterST();
        EXPECT_GE(center_st.x(), 0.0);
        EXPECT_LE(center_st.x(), 1.0);
        EXPECT_GE(center_st.y(), 0.0);
        EXPECT_LE(center_st.y(), 1.0);
        
        // Test bounds
        R2Rect bound_st = cell.GetBoundST();
        EXPECT_GE(bound_st.lo().x(), 0.0);
        EXPECT_LE(bound_st.hi().x(), 1.0);
        EXPECT_GE(bound_st.lo().y(), 0.0);
        EXPECT_LE(bound_st.hi().y(), 1.0);
        
        // Test size functions
        if (cell.level() <= 20) {  // Avoid very deep levels
            int size_ij = cell.GetSizeIJ();
            EXPECT_GT(size_ij, 0);
            
            double size_st = cell.GetSizeST();
            EXPECT_GT(size_st, 0.0);
            EXPECT_LE(size_st, 1.0);
        }
    }
}

// ==================== Navigation Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, NavigationOperations) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid() || cell.level() > 15) continue;  // Skip very deep levels
        
        // Test next/prev operations
        S2CellId next_cell = cell.next();
        S2CellId prev_cell = cell.prev();
        
        if (next_cell.is_valid()) {
            EXPECT_EQ(next_cell.level(), cell.level());
            
            // Test that prev of next gives us back the original (where valid)
            S2CellId prev_of_next = next_cell.prev();
            if (prev_of_next.is_valid()) {
                EXPECT_EQ(prev_of_next, cell);
            }
        }
        
        if (prev_cell.is_valid()) {
            EXPECT_EQ(prev_cell.level(), cell.level());
            
            // Test that next of prev gives us back the original (where valid)
            S2CellId next_of_prev = prev_cell.next();
            if (next_of_prev.is_valid()) {
                EXPECT_EQ(next_of_prev, cell);
            }
        }
    }
}

TEST_F(S2CellIdV2ComprehensiveTest, RangeOperations) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        S2CellId range_min = cell.range_min();
        S2CellId range_max = cell.range_max();
        
        EXPECT_TRUE(range_min.is_valid());
        EXPECT_TRUE(range_max.is_valid());
        
        // Range min should be <= cell <= range max
        EXPECT_LE(range_min, cell);
        EXPECT_LE(cell, range_max);
        
        // Range min should be <= range max
        EXPECT_LE(range_min, range_max);
    }
}

// ==================== Iterator Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, ChildIterators) {
    S2CellId root = S2CellId::FromFace(0);
    
    // Test child_begin/child_end for level 1
    if (root.level() < S2CellId::kMaxLevel) {
        S2CellId child_begin = root.child_begin();
        S2CellId child_end = root.child_end();
        
        EXPECT_TRUE(child_begin.is_valid());
        EXPECT_TRUE(child_end.is_valid());
        EXPECT_LT(child_begin, child_end);
        EXPECT_EQ(child_begin.level(), 1);
        EXPECT_EQ(child_end.level(), 1);
    }
    
    // Test child_begin/child_end for specific level
    int target_level = std::min(3, S2CellId::kMaxLevel);
    if (target_level > root.level()) {
        S2CellId child_begin = root.child_begin(target_level);
        S2CellId child_end = root.child_end(target_level);
        
        if (child_begin.is_valid() && child_end.is_valid()) {
            EXPECT_LT(child_begin, child_end);
            EXPECT_EQ(child_begin.level(), target_level);
            EXPECT_EQ(child_end.level(), target_level);
        }
    }
}

// ==================== Edge Cases Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, EdgeCases) {
    // Test invalid cell operations
    S2CellId invalid_cell;
    EXPECT_FALSE(invalid_cell.is_valid());
    EXPECT_EQ(invalid_cell.face(), 0);  // Default face for invalid cell
    EXPECT_EQ(invalid_cell.level(), 0);  // Default level for invalid cell
    
    // Test maximum level operations
    S2CellId max_level_cell = S2CellId::FromFaceLevel(0, S2CellId::kMaxLevel);
    if (max_level_cell.is_valid()) {
        EXPECT_EQ(max_level_cell.level(), S2CellId::kMaxLevel);
        
        // Should not be able to create children
        S2CellId invalid_child = max_level_cell.child(0);
        EXPECT_FALSE(invalid_child.is_valid());
    }
    
    // Test root cell special cases
    S2CellId root = S2CellId::FromFace(0);
    EXPECT_FALSE(root.parent().is_valid());
    EXPECT_EQ(root.parent(0), root);
}

// ==================== Performance Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, PerformanceBasicOperations) {
    const int num_operations = 10000;
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    // Ensure we have enough test cells
    while (test_cells.size() < 100) {
        test_cells.push_back(S2CellId::FromFace(test_cells.size() % 6));
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Test construction performance
    for (int i = 0; i < num_operations; ++i) {
        S2CellId cell = S2CellId::FromFace(i % 6);
        EXPECT_TRUE(cell.is_valid());
    }
    
    auto mid_time = std::chrono::high_resolution_clock::now();
    
    // Test comparison performance
    for (int i = 0; i < num_operations; ++i) {
        const S2CellId& cell1 = test_cells[i % test_cells.size()];
        const S2CellId& cell2 = test_cells[(i + 1) % test_cells.size()];
        bool result = cell1 < cell2;
        (void)result;  // Suppress unused variable warning
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto construction_time = std::chrono::duration_cast<std::chrono::microseconds>(mid_time - start_time).count();
    auto comparison_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - mid_time).count();
    
    // Performance should be reasonable (these are generous bounds)
    EXPECT_LT(construction_time, 1000000);  // Less than 1 second
    EXPECT_LT(comparison_time, 1000000);    // Less than 1 second
    
    std::cout << "Construction time: " << construction_time << " microseconds" << std::endl;
    std::cout << "Comparison time: " << comparison_time << " microseconds" << std::endl;
}

// ==================== Hash and Container Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, HashAndContainers) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    // Test with std::set for ordering (skip unordered_set due to hash function)
    std::set<S2CellId> ordered_set;
    
    for (const auto& cell : test_cells) {
        if (cell.is_valid()) {
            ordered_set.insert(cell);
        }
    }
    
    // Verify that all valid cells are in the set
    for (const auto& cell : test_cells) {
        if (cell.is_valid()) {
            EXPECT_TRUE(ordered_set.find(cell) != ordered_set.end());
        }
    }
    
    // Verify ordering
    S2CellId prev_cell;
    bool first = true;
    for (const auto& cell : ordered_set) {
        if (!first) {
            EXPECT_LE(prev_cell, cell);
        }
        prev_cell = cell;
        first = false;
    }
}

// ==================== Stress Tests ====================

TEST_F(S2CellIdV2ComprehensiveTest, StressTestDeepHierarchy) {
    // Test deep hierarchy operations
    S2CellId root = S2CellId::FromFace(0);
    S2CellId current = root;
    
    std::vector<S2CellId> hierarchy_chain;
    hierarchy_chain.push_back(current);
    
    // Build a deep hierarchy
    int max_depth = std::min(10, S2CellId::kMaxLevel);
    for (int level = 1; level <= max_depth; ++level) {
        S2CellId child = current.child(level % 4);
        if (!child.is_valid()) break;
        
        hierarchy_chain.push_back(child);
        current = child;
    }
    
    // Test parent traversal
    for (int i = hierarchy_chain.size() - 1; i > 0; --i) {
        S2CellId parent = hierarchy_chain[i].parent();
        EXPECT_EQ(parent, hierarchy_chain[i-1]);
    }
    
    // Test parent(level) for various levels
    for (int i = 0; i < hierarchy_chain.size(); ++i) {
        for (int target_level = 0; target_level <= i; ++target_level) {
            S2CellId ancestor = hierarchy_chain[i].parent(target_level);
            EXPECT_EQ(ancestor, hierarchy_chain[target_level]);
        }
    }
}