#include <iostream>
#include "s2/s2cell_id.h"
#include "s2/s2cell_id_v1.h"
#include "s2/s2latlng.h"
#include "s2/s2point.h"

int main() {
    try {
        // Test new S2CellId (formerly S2CellIdV2)
        std::cout << "Testing new S2CellId..." << std::endl;
        
        // Test default constructor
        S2CellId id;
        std::cout << "Default S2CellId created" << std::endl;
        
        // Test from coordinates
        S2Point p(1.0, 0.0, 0.0);
        S2CellId id_from_point(p);
        std::cout << "S2CellId from S2Point: " << id_from_point.id() << std::endl;
        
        // Test from lat/lng
        S2LatLng ll = S2LatLng::FromDegrees(37.7749, -122.4194); // San Francisco
        S2CellId id_from_latlng(ll);
        std::cout << "S2CellId from S2LatLng: " << id_from_latlng.id() << std::endl;
        
        // Test level operations
        std::cout << "Level: " << id_from_latlng.level() << std::endl;
        
        // Test old S2CellId (now in s2v1 namespace)
        std::cout << "\nTesting old S2CellId (s2v1)..." << std::endl;
        s2v1::S2CellId old_id;
        std::cout << "Old S2CellId created" << std::endl;
        
        s2v1::S2CellId old_id_from_point(p);
        std::cout << "Old S2CellId from S2Point: " << old_id_from_point.id() << std::endl;
        
        std::cout << "\nAll tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
