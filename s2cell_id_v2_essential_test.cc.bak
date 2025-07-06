#include "src/s2/s2cell_id.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <string>

class S2CellIdV2EssentialTest : public ::testing::Test {
protected:
    std::vector<S2CellId> CreateTestCells() {
        std::vector<S2CellId> cells;
        
        // Add face cells (level 0)
        for (int face = 0; face < 6; ++face) {
            cells.push_back(S2CellId::FromFace(face));
        }
        
        // Add cells at different levels
        for (int level = 1; level <= std::min(5, S2CellId::kMaxLevel); ++level) {
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

// ==================== Essential Construction Tests ====================

TEST_F(S2CellIdV2EssentialTest, DefaultConstructor) {
    S2CellId default_cell;
    EXPECT_FALSE(default_cell.is_valid());
    EXPECT_EQ(default_cell.new_id(), 0);
}

TEST_F(S2CellIdV2EssentialTest, FromFace) {
    for (int face = 0; face < 6; ++face) {
        S2CellId cell = S2CellId::FromFace(face);
        EXPECT_TRUE(cell.is_valid());
        EXPECT_EQ(cell.face(), face);
        EXPECT_EQ(cell.level(), 0);
        EXPECT_TRUE(cell.is_face());
        EXPECT_FALSE(cell.is_leaf());
    }
}

TEST_F(S2CellIdV2EssentialTest, FromFaceLevel) {
    for (int face = 0; face < 6; ++face) {
        for (int level = 0; level <= std::min(5, S2CellId::kMaxLevel); ++level) {
            S2CellId cell = S2CellId::FromFaceLevel(face, level);
            EXPECT_TRUE(cell.is_valid());
            EXPECT_EQ(cell.face(), face);
            EXPECT_EQ(cell.level(), level);
        }
    }
}

TEST_F(S2CellIdV2EssentialTest, FromPoint) {
    std::vector<S2Point> test_points = {
        S2Point(1, 0, 0),    // On face 0
        S2Point(0, 1, 0),    // On face 1
        S2Point(0, 0, 1),    // On face 2
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

TEST_F(S2CellIdV2EssentialTest, TokenRoundTrip) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        std::string token = cell.ToToken();
        EXPECT_FALSE(token.empty());
        
        S2CellId parsed = S2CellId::FromToken(token);
        
        if (parsed.is_valid()) {
            EXPECT_EQ(cell.face(), parsed.face());
            EXPECT_EQ(cell.level(), parsed.level());
        }
    }
}

// ==================== Compatibility Tests ====================

TEST_F(S2CellIdV2EssentialTest, OriginalS2CellIdCompatibility) {
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

// ==================== Navigation Tests ====================

TEST_F(S2CellIdV2EssentialTest, ParentChild) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        // Test parent
        if (cell.level() > 0) {
            S2CellId parent = cell.parent();
            EXPECT_TRUE(parent.is_valid());
            EXPECT_EQ(parent.level(), cell.level() - 1);
        }
        
        // Test children
        if (cell.level() < S2CellId::kMaxLevel) {
            for (int i = 0; i < 4; ++i) {
                S2CellId child = cell.child(i);
                if (child.is_valid()) {
                    EXPECT_EQ(child.level(), cell.level() + 1);
                    EXPECT_EQ(child.parent(), cell);
                }
            }
        }
    }
}

TEST_F(S2CellIdV2EssentialTest, RangeOperations) {
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

// ==================== String Conversion Tests ====================

TEST_F(S2CellIdV2EssentialTest, ToStringFromString) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        std::string str = cell.ToString();
        EXPECT_FALSE(str.empty());
        EXPECT_NE(str, "INVALID");
        
        // Test parsing back
        S2CellId parsed = S2CellId::FromString(str);
        if (parsed.is_valid()) {
            EXPECT_EQ(cell.face(), parsed.face());
            EXPECT_EQ(cell.level(), parsed.level());
        }
    }
    
    // Test edge cases
    EXPECT_EQ(S2CellId().ToString(), "INVALID");
    EXPECT_FALSE(S2CellId::FromString("").is_valid());
    EXPECT_FALSE(S2CellId::FromString("invalid").is_valid());
}

// ==================== Edge Cases Tests ====================

TEST_F(S2CellIdV2EssentialTest, InvalidInputHandling) {
    // Test invalid face
    S2CellId invalid_face = S2CellId::FromFaceLevel(-1, 0);
    EXPECT_FALSE(invalid_face.is_valid());
    
    invalid_face = S2CellId::FromFaceLevel(6, 0);
    EXPECT_FALSE(invalid_face.is_valid());
    
    // Test invalid level
    S2CellId invalid_level = S2CellId::FromFaceLevel(0, -1);
    EXPECT_FALSE(invalid_level.is_valid());
    
    invalid_level = S2CellId::FromFaceLevel(0, S2CellId::kMaxLevel + 1);
    EXPECT_FALSE(invalid_level.is_valid());
    
    // Test invalid child position
    S2CellId valid_cell = S2CellId::FromFaceLevel(0, 1);
    S2CellId invalid_child = valid_cell.child(-1);
    EXPECT_FALSE(invalid_child.is_valid());
    
    invalid_child = valid_cell.child(4);
    EXPECT_FALSE(invalid_child.is_valid());
    
    // Test operations on invalid cells
    S2CellId invalid_cell;
    EXPECT_FALSE(invalid_cell.parent().is_valid());
    EXPECT_FALSE(invalid_cell.child(0).is_valid());
    EXPECT_FALSE(invalid_cell.next().is_valid());
    EXPECT_FALSE(invalid_cell.prev().is_valid());
}

// ==================== Geometric Properties Tests ====================

TEST_F(S2CellIdV2EssentialTest, BasicGeometricProperties) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid()) continue;
        
        // Test point conversion
        S2Point point = cell.ToPointRaw();
        EXPECT_TRUE(std::isfinite(point.x()) && std::isfinite(point.y()) && std::isfinite(point.z()));
        
        // Test center coordinates
        R2Point center_st = cell.GetCenterST();
        EXPECT_GE(center_st.x(), 0.0);
        EXPECT_LE(center_st.x(), 1.0);
        EXPECT_GE(center_st.y(), 0.0);
        EXPECT_LE(center_st.y(), 1.0);
        
        // Test size functions
        if (cell.level() <= 10) {
            int size_ij = cell.GetSizeIJ();
            EXPECT_GT(size_ij, 0);
            
            double size_st = cell.GetSizeST();
            EXPECT_GT(size_st, 0.0);
            EXPECT_LE(size_st, 1.0);
        }
    }
}

// ==================== Containment Tests ====================

TEST_F(S2CellIdV2EssentialTest, ContainsIntersects) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid() || cell.level() >= S2CellId::kMaxLevel) continue;
        
        // Test with children
        for (int i = 0; i < 4; ++i) {
            S2CellId child = cell.child(i);
            if (child.is_valid()) {
                EXPECT_TRUE(cell.contains(child));
                EXPECT_TRUE(cell.intersects(child));
                EXPECT_TRUE(child.intersects(cell));
            }
        }
        
        // Test with self
        EXPECT_TRUE(cell.contains(cell));
        EXPECT_TRUE(cell.intersects(cell));
    }
}

// ==================== Edge Neighbor Tests ====================

TEST_F(S2CellIdV2EssentialTest, EdgeNeighbors) {
    std::vector<S2CellId> test_cells = CreateTestCells();
    
    for (const auto& cell : test_cells) {
        if (!cell.is_valid() || cell.level() > 3) continue;  // Keep it simple
        
        S2CellId neighbors[4];
        cell.GetEdgeNeighbors(neighbors);
        
        for (int i = 0; i < 4; ++i) {
            if (neighbors[i].is_valid()) {
                EXPECT_EQ(neighbors[i].level(), cell.level());
                EXPECT_LE(neighbors[i].level(), S2CellId::kMaxLevel);
                EXPECT_NE(neighbors[i], cell);  // Should be different from original
            }
        }
    }
}
