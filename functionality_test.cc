#include "s2/s2cell_id.h"
#include "s2/s2cell_id_v1.h"
#include "s2/s2latlng.h"
#include "s2/s2point.h"
#include <iostream>
#include <cassert>

void TestBasicConstruction() {
    std::cout << "Testing basic construction..." << std::endl;
    
    // Test default constructor
    S2CellId id;
    assert(!id.is_valid());
    
    // Test FromFace
    S2CellId face_id = S2CellId::FromFace(0);
    assert(face_id.is_valid());
    assert(face_id.face() == 0);
    assert(face_id.level() == 0);
    
    // Test FromPoint
    S2Point p(1.0, 0.0, 0.0);
    S2CellId point_id(p);
    assert(point_id.is_valid());
    
    std::cout << "Basic construction tests passed!" << std::endl;
}

void TestConversion() {
    std::cout << "Testing conversion between old and new formats..." << std::endl;
    
    // Create old format ID
    s2v1::S2CellId old_id = s2v1::S2CellId::FromFace(1);
    
    // Convert to new format
    S2CellId new_id(old_id);
    assert(new_id.is_valid());
    assert(new_id.face() == 1);
    assert(new_id.level() == 0);
    
    // Convert back to old format
    s2v1::S2CellId converted_back = new_id.ToOldFormat();
    assert(converted_back.is_valid());
    assert(converted_back.face() == 1);
    assert(converted_back.level() == 0);
    
    std::cout << "Conversion tests passed!" << std::endl;
}

void TestLevelOperations() {
    std::cout << "Testing level operations..." << std::endl;
    
    // Create a cell and test parent/child operations
    S2CellId face_id = S2CellId::FromFace(2);
    assert(face_id.level() == 0);
    assert(face_id.is_face());
    
    // Test child operations
    S2CellId child = face_id.child(0);
    assert(child.is_valid());
    assert(child.level() == 1);
    assert(child.face() == 2);
    assert(child.child_position() == 0);
    
    // Test parent operations
    S2CellId parent = child.parent();
    assert(parent.is_valid());
    assert(parent.level() == 0);
    assert(parent.face() == 2);
    
    std::cout << "Level operations tests passed!" << std::endl;
}

void TestStringOperations() {
    std::cout << "Testing string operations..." << std::endl;
    
    // Test ToString and FromString
    S2CellId id = S2CellId::FromFace(3);
    std::string str = id.ToString();
    assert(str == "3");
    
    S2CellId from_str = S2CellId::FromString(str);
    assert(from_str.is_valid());
    assert(from_str.face() == 3);
    assert(from_str.level() == 0);
    
    // Test with child cell
    S2CellId child = id.child(2);
    std::string child_str = child.ToString();
    assert(child_str == "3/2");
    
    S2CellId from_child_str = S2CellId::FromString(child_str);
    assert(from_child_str.is_valid());
    assert(from_child_str.face() == 3);
    assert(from_child_str.level() == 1);
    assert(from_child_str.child_position() == 2);
    
    std::cout << "String operations tests passed!" << std::endl;
}

void TestComparison() {
    std::cout << "Testing comparison operations..." << std::endl;
    
    S2CellId id1 = S2CellId::FromFace(0);
    S2CellId id2 = S2CellId::FromFace(0);
    S2CellId id3 = S2CellId::FromFace(1);
    
    assert(id1 == id2);
    assert(!(id1 == id3));
    assert(id1 != id3);
    
    std::cout << "Comparison tests passed!" << std::endl;
}

int main() {
    try {
        TestBasicConstruction();
        TestConversion();
        TestLevelOperations();
        TestStringOperations();
        TestComparison();
        
        std::cout << "\n=== All functionality tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
