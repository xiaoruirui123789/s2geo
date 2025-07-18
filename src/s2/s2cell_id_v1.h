// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2CELL_ID_V1_H_
#define S2_S2CELL_ID_V1_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/numeric/bits.h"
#include "absl/strings/string_view.h"

#include "s2/_fp_contract_off.h"  // IWYU pragma: keep
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s1angle.h"
#include "s2/s2coder.h"
#include "s2/s2coords.h"
#include "s2/s2error.h"
#include "s2/s2point.h"
#include "s2/s2region.h"
#include "s2/s2latlng.h"
#include "s2/util/bits/bits.h"
#include "s2/util/coding/coder.h"

// 前向声明 S2LatLng，然后创建类型别名
class S2LatLng;

namespace s2v1 {
  
// 在 s2v1 命名空间中创建 S2LatLng 的类型别名
using S2LatLng = ::S2LatLng;

// SWIG does not understand attribute syntax.
#ifndef SWIG
#define IFNDEF_SWIG(x) x
#else
#define IFNDEF_SWIG(x)
#endif

// An S2CellId is a 64-bit unsigned integer that uniquely identifies a
// cell in the S2 cell decomposition.  It has the following format:
//
//   id = [face][face_pos]
//
//   face:     a 3-bit number (range 0..5) encoding the cube face.
//
//   face_pos: a 61-bit number encoding the position of the center of this
//             cell along the Hilbert curve over this face (see the Wiki
//             pages for details).
//
// Sequentially increasing cell ids follow a continuous space-filling curve
// over the entire sphere.  They have the following properties:
//
//  - The id of a cell at level k consists of a 3-bit face number followed
//    by k bit pairs that recursively select one of the four children of
//    each cell.  The next bit is always 1, and all other bits are 0.
//    Therefore, the level of a cell is determined by the position of its
//    lowest-numbered bit that is turned on (for a cell at level k, this
//    position is 2 * (kMaxLevel - k).)
//
//  - The id of a parent cell is at the midpoint of the range of ids spanned
//    by its children (or by its descendants at any level).
//
// Leaf cells are often used to represent points on the unit sphere, and
// this class provides methods for converting directly between these two
// representations.  For cells that represent 2D regions rather than
// discrete point, it is better to use the S2Cell class.
//
// All methods require `is_valid()` to be true unless otherwise specified
// (although not all methods enforce this).
//
// This class is intended to be copied by value as desired.  It uses
// the default copy constructor and assignment operator.
class S2CellId {
 public:
  // Although only 60 bits are needed to represent the index of a leaf cell, the
  // extra position bit lets us encode each cell as its Hilbert curve position
  // at the cell center, which is halfway along the portion of the Hilbert curve
  // that fills that cell.
  static constexpr int kFaceBits = 3;
  static constexpr int kNumFaces = 6;
  static constexpr int kMaxLevel =
      S2::kMaxCellLevel;  // Valid levels: 0..kMaxLevel
  static constexpr int kPosBits = 2 * kMaxLevel + 1;
  static constexpr int kMaxSize = 1 << kMaxLevel;

  explicit constexpr S2CellId(uint64_t id) : id_(id) {}

  // Construct a leaf cell containing the given point "p".  Usually there is
  // exactly one such cell, but for points along the edge of a cell, any
  // adjacent cell may be (deterministically) chosen.  This is because
  // S2CellIds are considered to be closed sets.  The returned cell will
  // always contain the given point, i.e.
  //
  //   S2Cell(S2CellId(p)).Contains(p)
  //
  // is always true.  The point "p" does not need to be normalized.
  //
  // If instead you want every point to be contained by exactly one S2Cell,
  // you will need to convert the S2CellIds to S2Loops (which implement point
  // containment this way).
  explicit S2CellId(const S2Point& p);

  // Construct a leaf cell containing the given normalized S2LatLng.
  // REQUIRES: Latitude and longitude are finite.
  explicit S2CellId(const S2LatLng& ll);

  // The default constructor returns an invalid cell id.
  constexpr S2CellId() = default;
  // Returns an invalid cell id.
  static constexpr S2CellId None() { return S2CellId(); }

  // Returns an invalid cell id guaranteed to be larger than any
  // valid cell id.  Useful for creating indexes.
  static constexpr S2CellId Sentinel() { return S2CellId(~uint64_t{0}); }

  // Return the cell corresponding to a given S2 cube face.
  static S2CellId FromFace(int face);

  // Return a cell given its face (range 0..5), Hilbert curve position within
  // that face (an unsigned integer with S2CellId::kPosBits bits), and level
  // (range 0..kMaxLevel).  The given position will be modified to correspond
  // to the Hilbert curve position at the center of the returned cell.  This
  // is a static function rather than a constructor in order to indicate what
  // the arguments represent.
  static S2CellId FromFacePosLevel(int face, uint64_t pos, int level);

  // Return the direction vector corresponding to the center of the given
  // cell.  The vector returned by ToPointRaw is not necessarily unit length.
  // This method returns the same result as S2Cell::GetCenter().
  //
  // The maximum directional error in ToPoint() (compared to the exact
  // mathematical result) is 1.5 * DBL_EPSILON radians, and the maximum length
  // error is 2 * DBL_EPSILON (the same as Normalize).
  S2Point ToPoint() const { return ToPointRaw().Normalize(); }
  S2Point ToPointRaw() const;

  // Return the center of the cell in (s,t) coordinates (see s2coords.h).
  R2Point GetCenterST() const;

  // Return the edge length of this cell in (s,t)-space.
  double GetSizeST() const;

  // Return the edge length in (s,t)-space of cells at the given level.
  static double GetSizeST(int level);

  // Return the bounds of this cell in (s,t)-space.
  R2Rect GetBoundST() const;

  // Return the center of the cell in (u,v) coordinates (see s2coords.h).
  // Note that the center of the cell is defined as the point at which it is
  // recursively subdivided into four children; in general, it is not at the
  // midpoint of the (u,v) rectangle covered by the cell.
  R2Point GetCenterUV() const;

  // Return the bounds of this cell in (u,v)-space.
  R2Rect GetBoundUV() const;

  // Expand a rectangle in (u,v)-space so that it contains all points within
  // the given distance of the boundary, and return the smallest such
  // rectangle.  If the distance is negative, then instead shrink this
  // rectangle so that it excludes all points within the given absolute
  // distance of the boundary.
  //
  // Distances are measured *on the sphere*, not in (u,v)-space.  For example,
  // you can use this method to expand the (u,v)-bound of an S2CellId so that
  // it contains all points within 5km of the original cell.  You can then
  // test whether a point lies within the expanded bounds like this:
  //
  //   R2Point uv;
  //   if (S2::FaceXYZtoUV(face, point, &uv) && bound.Contains(uv)) { ... }
  //
  // Limitations:
  //
  //  - Because the rectangle is drawn on one of the six cube-face planes
  //    (i.e., {x,y,z} = +/-1), it can cover at most one hemisphere.  This
  //    limits the maximum amount that a rectangle can be expanded.  For
  //    example, S2CellId bounds can be expanded safely by at most 45 degrees
  //    (about 5000 km on the Earth's surface).
  //
  //  - The implementation is not exact for negative distances.  The resulting
  //    rectangle will exclude all points within the given distance of the
  //    boundary but may be slightly smaller than necessary.
  static R2Rect ExpandedByDistanceUV(const R2Rect& uv, S1Angle distance);

  // Return the (face, si, ti) coordinates of the center of the cell.  Note
  // that although (si,ti) coordinates span the range [0,2**31] in general,
  // the cell center coordinates are always in the range [1,2**31-1] and
  // therefore can be represented using a signed 32-bit integer.
  int GetCenterSiTi(int* psi, int* pti) const;

  // Return the S2LatLng corresponding to the center of the given cell.
  S2LatLng ToLatLng() const;

  // The 64-bit unique identifier for this cell.
  constexpr uint64_t id() const { return id_; }

  // Return true if id() represents a valid cell.
  //
  // All methods require is_valid() to be true unless otherwise specified
  // (although not all methods enforce this).
  bool is_valid() const;

  // Which cube face this cell belongs to, in the range 0..5.
  int face() const;

  // The position of the cell center along the Hilbert curve over this face,
  // in the range 0..(2**kPosBits-1).
  uint64_t pos() const;

  // Return the subdivision level of the cell (range 0..kMaxLevel).
  int level() const;

  // Return the edge length of this cell in (i,j)-space.
  int GetSizeIJ() const;

  // Like the above, but return the size of cells at the given level.
  static int GetSizeIJ(int level);

  // Return true if this is a leaf cell (more efficient than checking
  // whether level() == kMaxLevel).
  bool is_leaf() const;

  // Return true if this is a top-level face cell (more efficient than
  // checking whether level() == 0).
  bool is_face() const;

  // Return the child position (0..3) of this cell within its parent.
  // REQUIRES: level() >= 1.
  int child_position() const;

  // Return the child position (0..3) of this cell's ancestor at the given
  // level within its parent.  For example, child_position(1) returns the
  // position of this cell's level-1 ancestor within its top-level face cell.
  // REQUIRES: 1 <= level <= this->level().
  int child_position(int level) const;

  // These methods return the range of cell ids that are contained within this
  // cell (including itself).  The range is *inclusive* (i.e. test using >=
  // and <=) and the return values of both methods are valid leaf cell ids.
  // In other words, a.contains(b) if and only if
  //
  //     (b >= a.range_min() && b <= a.range_max())
  //
  // If you want to iterate through all the descendants of this cell at a
  // particular level, use child_begin(level) and child_end(level) instead.
  // Also see maximum_tile(), which can be used to iterate through a range of
  // cells using S2CellIds at different levels that are as large as possible.
  //
  // If you need to convert the range to a semi-open interval [min, limit)
  // (e.g., in order to use a key-value store that only supports semi-open
  // range queries), do not attempt to define "limit" as range_max.next().
  // The problem is that leaf S2CellIds are 2 units apart, so the semi-open
  // interval [min, limit) includes an additional value (range_max.id() + 1)
  // which happens to be a valid S2CellId about one-third of the time and
  // is *never* contained by this cell.  (It always corresponds to a cell that
  // is larger than this one.)  You can define "limit" as (range_max.id() + 1)
  // if necessary (which is not always a valid S2CellId but can still be used
  // with FromToken/ToToken), or you can convert range_max() to the key space
  // of your key-value store and define "limit" as Successor(key).
  //
  // Note that Sentinel().range_min() == Sentinel.range_max() == Sentinel().
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId range_min() const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId range_max() const;

  // Return true if the given cell is contained within this one.
  bool contains(S2CellId other) const;

  // Return true if the given cell intersects this one.
  bool intersects(S2CellId other) const;

  // Return the cell at the previous level or at the given level (which must
  // be less than or equal to the current level).
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId parent() const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId parent(int level) const;

  // Return the immediate child of this cell at the given traversal order
  // position (in the range 0 to 3).  This cell must not be a leaf cell.
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId child(int position) const;

  // Iterator-style methods for traversing the immediate children of a cell or
  // all of the children at a given level (greater than or equal to the current
  // level).  Note that the end value is exclusive, just like standard STL
  // iterators, and may not even be a valid cell id.  You should iterate using
  // code like this:
  //
  //   for(S2CellId c = id.child_begin(); c != id.child_end(); c = c.next())
  //     ...
  //
  // The convention for advancing the iterator is "c = c.next()" rather
  // than "++c" to avoid possible confusion with incrementing the
  // underlying 64-bit cell id.
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId child_begin() const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId child_begin(int level) const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId child_end() const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId child_end(int level) const;

  // Return the next/previous cell at the same level along the Hilbert curve.
  // Works correctly when advancing from one face to the next, but
  // does *not* wrap around from the last face to the first or vice versa.
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId next() const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId prev() const;

  // This method advances or retreats the indicated number of steps along the
  // Hilbert curve at the current level, and returns the new position.  The
  // position is never advanced past End() or before Begin().
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId advance(int64_t steps) const;

  // Returns the number of steps that this cell is from Begin(level()). The
  // return value is always non-negative.
  int64_t distance_from_begin() const;

  // Like next() and prev(), but these methods wrap around from the last face
  // to the first and vice versa.  They should *not* be used for iteration in
  // conjunction with child_begin(), child_end(), Begin(), or End().  The
  // input must be a valid cell id.
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId next_wrap() const;
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId prev_wrap() const;

  // This method advances or retreats the indicated number of steps along the
  // Hilbert curve at the current level, and returns the new position.  The
  // position wraps between the first and last faces as necessary.  The input
  // must be a valid cell id.
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId advance_wrap(int64_t steps) const;

  // Return the largest cell with the same range_min() and such that
  // range_max() < limit.range_min().  Returns "limit" if no such cell exists.
  // This method can be used to generate a small set of S2CellIds that covers
  // a given range (a "tiling").  This example shows how to generate a tiling
  // for a semi-open range of leaf cells [start, limit):
  //
  //   for (S2CellId id = start.maximum_tile(limit);
  //        id != limit; id = id.next().maximum_tile(limit)) { ... }
  //
  // Note that in general the cells in the tiling will be of different sizes;
  // they gradually get larger (near the middle of the range) and then
  // gradually get smaller (as "limit" is approached).
  IFNDEF_SWIG(ABSL_MUST_USE_RESULT) S2CellId maximum_tile(S2CellId limit) const;

  // Returns the level of the lowest common ancestor of this cell and "other",
  // i.e. the maximum level where this->parent(level) == other.parent(level).
  // Note that this definition also covers the situation where this cell is a
  // descendant of "other" or vice versa, or the two cells are the same,
  // since this->parent(this->level()) == *this.
  //
  // Returns -1 if the two cells do not have any common ancestor (i.e., they
  // are from different faces).
  int GetCommonAncestorLevel(S2CellId other) const;

  // Iterator-style methods for traversing all the cells along the Hilbert
  // curve at a given level (across all 6 faces of the cube).  Note that the
  // end value is exclusive (just like standard STL iterators), and is not a
  // valid cell id.
  static S2CellId Begin(int level);
  static S2CellId End(int level);

  // Methods to encode and decode cell ids to compact text strings suitable
  // for display or indexing.  Cells at lower levels (i.e. larger cells) are
  // encoded into fewer characters.  The maximum token length is 16.
  //
  // Tokens preserve ordering, i.e. ToToken(x) < ToToken(y) iff x < y.
  //
  // ToToken() returns a string by value for convenience; the compiler
  // does this without intermediate copying in most cases.
  //
  // These methods guarantee that FromToken(ToToken(x)) == x even when
  // "x" is an invalid cell id.  All tokens are alphanumeric strings.
  // FromToken() returns S2CellId::None() for malformed inputs.
  std::string ToToken() const;
  static S2CellId FromToken(absl::string_view token);

  // Legacy coder for S2CellId that delegates to the token representation.
  // Storage is variable depending on the level of the cell.
  class Coder : public s2coding::S2Coder<S2CellId> {
   public:
    void Encode(Encoder& encoder, const S2CellId& v) const override;
    bool Decode(Decoder& decoder, S2CellId& v, S2Error& error) const override;
  };

  // Use encoder to generate a serialized representation of this cell id.
  // Can also encode an invalid cell.
  void Encode(Encoder* encoder) const;

  // Decodes an S2CellId encoded by Encode(). Returns true on success.
  bool Decode(Decoder* decoder);

  // Creates a human readable debug string.  Used for << and available for
  // direct usage as well.  The format is "f/dd..d" where "f" is a digit in
  // the range [0-5] representing the S2CellId face, and "dd..d" is a string
  // of digits in the range [0-3] representing each child's position with
  // respect to its parent.  (Note that the latter string may be empty.)
  //
  // For example "4/" represents S2CellId::FromFace(4), and "3/02" represents
  // S2CellId::FromFace(3).child(0).child(2).
  std::string ToString() const;

  // Converts a string in the format returned by ToString() to an S2CellId.
  // Returns S2CellId::None() if the string could not be parsed.
  //
  // The method name includes "Debug" in order to avoid possible confusion
  // with FromToken() above.
  static S2CellId FromDebugString(absl::string_view str);

  // Return the four cells that are adjacent across the cell's four edges.
  // Neighbors are returned in the order defined by S2Cell::GetEdge.  All
  // neighbors are guaranteed to be distinct.
  void GetEdgeNeighbors(S2CellId neighbors[4]) const;

  // Return the S2CellIds of the neighbors of the closest vertex to this cell
  // at the given level, by appending them to "output".  Normally there are four
  // neighbors, but the closest vertex may only have three neighbors if it is
  // one of the 8 cube vertices.
  //
  // Requires: level < this->level(), so that we can determine which vertex is
  // closest (in particular, level == kMaxLevel is not allowed).
  void AppendVertexNeighbors(int level, std::vector<S2CellId>* output) const;

  // Append all neighbors of this cell at the given level to "output".  Two
  // cells X and Y are neighbors if their boundaries intersect but their
  // interiors do not.  In particular, two cells that intersect at a single
  // point are neighbors.  Note that for cells adjacent to a face vertex, the
  // same neighbor may be appended more than once.
  //
  // REQUIRES: nbr_level >= this->level().
  void AppendAllNeighbors(int nbr_level, std::vector<S2CellId>* output) const;

  /////////////////////////////////////////////////////////////////////
  // Low-level methods.

  // Return a leaf cell given its cube face (range 0..5) and
  // i- and j-coordinates (see s2coords.h).
  static S2CellId FromFaceIJ(int face, int i, int j);

  // Return the (face, i, j) coordinates for the leaf cell corresponding to
  // this cell id.  Since cells are represented by the Hilbert curve position
  // at the center of the cell, the returned (i,j) for non-leaf cells will be
  // a leaf cell adjacent to the cell center.  If "orientation" is non-nullptr,
  // also return the Hilbert curve orientation for the current cell.
  int ToFaceIJOrientation(int* pi, int* pj, int* orientation) const;

  // Return the lowest-numbered bit that is on for this cell id, which is
  // equal to (uint64_t{1} << (2 * (kMaxLevel - level))).  So for example,
  // a.lsb() <= b.lsb() if and only if a.level() >= b.level(), but the
  // first test is more efficient.
  uint64_t lsb() const { return id_ & (~id_ + 1); }

  // Return the lowest-numbered bit that is on for cells at the given level.
  static constexpr uint64_t lsb_for_level(int level) {
    return uint64_t{1} << (2 * (kMaxLevel - level));
  }

  // Return the bound in (u,v)-space for the cell at the given level containing
  // the leaf cell with the given (i,j)-coordinates.
  static R2Rect IJLevelToBoundUV(int ij[2], int level);

  // When S2CellId is used as a key in one of the absl::btree container types,
  // indicate that linear rather than binary search should be used.  This is
  // much faster when the comparison function is cheap.
  typedef std::true_type absl_btree_prefer_linear_node_search;

 private:
  // This is the offset required to wrap around from the beginning of the
  // Hilbert curve to the end or vice versa; see next_wrap() and prev_wrap().
  // SWIG doesn't understand uint64_t{}, so use static_cast.
  static constexpr uint64_t kWrapOffset =
      static_cast<uint64_t>(kNumFaces) << kPosBits;

  // Given a face and a point (i,j) where either i or j is outside the valid
  // range [0..kMaxSize-1], this function first determines which neighboring
  // face "contains" (i,j), and then returns the leaf cell on that face which
  // is adjacent to the given face and whose distance from (i,j) is minimal.
  static S2CellId FromFaceIJWrap(int face, int i, int j);

  // Inline helper function that calls FromFaceIJ if "same_face" is true,
  // or FromFaceIJWrap if "same_face" is false.
  static S2CellId FromFaceIJSame(int face, int i, int j, bool same_face);

  uint64_t id_ = 0;       // 0 is an invalid cell id.
} ABSL_ATTRIBUTE_PACKED;  // Necessary so that structures containing S2CellId's
                          // can be ABSL_ATTRIBUTE_PACKED.

inline constexpr bool operator==(S2CellId x, S2CellId y) {
  return x.id() == y.id();
}

inline constexpr bool operator!=(S2CellId x, S2CellId y) {
  return x.id() != y.id();
}

inline constexpr bool operator<(S2CellId x, S2CellId y) {
  return x.id() < y.id();
}

inline constexpr bool operator>(S2CellId x, S2CellId y) {
  return x.id() > y.id();
}

inline constexpr bool operator<=(S2CellId x, S2CellId y) {
  return x.id() <= y.id();
}

inline constexpr bool operator>=(S2CellId x, S2CellId y) {
  return x.id() >= y.id();
}

inline S2CellId S2CellId::FromFace(int face) {
  return S2CellId((static_cast<uint64_t>(face) << kPosBits) + lsb_for_level(0));
}

inline S2CellId S2CellId::FromFacePosLevel(int face, uint64_t pos, int level) {
  S2CellId cell((static_cast<uint64_t>(face) << kPosBits) + (pos | 1));
  // `parent` `ABSL_DCHECK`s level, so don't do it here.
  return cell.parent(level);
}

inline int S2CellId::GetCenterSiTi(int* psi, int* pti) const {
  // First we compute the discrete (i,j) coordinates of a leaf cell contained
  // within the given cell.  Given that cells are represented by the Hilbert
  // curve position corresponding at their center, it turns out that the cell
  // returned by ToFaceIJOrientation is always one of two leaf cells closest
  // to the center of the cell (unless the given cell is a leaf cell itself,
  // in which case there is only one possibility).
  //
  // Given a cell of size s >= 2 (i.e. not a leaf cell), and letting (imin,
  // jmin) be the coordinates of its lower left-hand corner, the leaf cell
  // returned by ToFaceIJOrientation() is either (imin + s/2, jmin + s/2)
  // (imin + s/2 - 1, jmin + s/2 - 1).  The first case is the one we want.
  // We can distinguish these two cases by looking at the low bit of "i" or
  // "j".  In the second case the low bit is one, unless s == 2 (i.e. the
  // level just above leaf cells) in which case the low bit is zero.
  //
  // In the code below, the expression ((i ^ (int(id_) >> 2)) & 1) is true
  // if we are in the second case described above.
  int i, j;
  int face = ToFaceIJOrientation(&i, &j, nullptr);
  int delta = is_leaf() ? 1 : ((i ^ (static_cast<int>(id_) >> 2)) & 1) ? 2 : 0;

  // Note that (2 * {i,j} + delta) will never overflow a 32-bit integer.
  *psi = 2 * i + delta;
  *pti = 2 * j + delta;
  return face;
}

inline bool S2CellId::is_valid() const {
  return (face() < kNumFaces && (lsb() & 0x1555555555555555ULL));
}

inline int S2CellId::face() const {
  return id_ >> kPosBits;
}

inline uint64_t S2CellId::pos() const {
  return id_ & (~uint64_t{0} >> kFaceBits);
}

inline int S2CellId::level() const {
  // We can't just ABSL_DCHECK(is_valid()) because we want level() to be
  // defined for end-iterators, i.e. S2CellId::End(kLevel).  However there is
  // no good way to define S2CellId::None().level(), so we do prohibit that.
  ABSL_ASSUME(id_ != 0);

  // A special case for leaf cells is not worthwhile.
  return kMaxLevel - (absl::countr_zero(id_) >> 1);
}

inline int S2CellId::GetSizeIJ() const {
  return GetSizeIJ(level());
}

inline double S2CellId::GetSizeST() const {
  return GetSizeST(level());
}

inline int S2CellId::GetSizeIJ(int level) {
  ABSL_DCHECK_GE(level, 0);
  ABSL_DCHECK_LE(level, kMaxLevel);
  return 1 << (kMaxLevel - level);
}

inline double S2CellId::GetSizeST(int level) {
  // `GetSizeIJ` `ABSL_DCHECK`s `level`, so don't do it here.
  return S2::IJtoSTMin(GetSizeIJ(level));
}

inline bool S2CellId::is_leaf() const { return id_ & 1; }

inline bool S2CellId::is_face() const {
  return (id_ & (lsb_for_level(0) - 1)) == 0;
}

inline int S2CellId::child_position() const {
  // No need for a special implementation; the compiler optimizes this well.
  return child_position(level());
}

inline int S2CellId::child_position(int level) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK_GE(level, 1);
  ABSL_DCHECK_LE(level, this->level());
  return static_cast<int>(id_ >> (2 * (kMaxLevel - level) + 1)) & 3;
}

inline S2CellId S2CellId::range_min() const {
  return S2CellId(id_ - (lsb() - 1));
}

inline S2CellId S2CellId::range_max() const {
  return S2CellId(id_ + (lsb() - 1));
}

inline bool S2CellId::contains(S2CellId other) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK(other.is_valid());
  return other >= range_min() && other <= range_max();
}

inline bool S2CellId::intersects(S2CellId other) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK(other.is_valid());
  return other.range_min() <= range_max() && other.range_max() >= range_min();
}

inline S2CellId S2CellId::parent(int level) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK_GE(level, 0);
  ABSL_DCHECK_LE(level, this->level());
  uint64_t new_lsb = lsb_for_level(level);
  return S2CellId((id_ & (~new_lsb + 1)) | new_lsb);
}

inline S2CellId S2CellId::parent() const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK(!is_face());
  uint64_t new_lsb = lsb() << 2;
  return S2CellId((id_ & (~new_lsb + 1)) | new_lsb);
}

inline S2CellId S2CellId::child(int position) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK(!is_leaf());
  // To change the level, we need to move the least-significant bit two
  // positions downward.  We do this by subtracting (4 * new_lsb) and adding
  // new_lsb.  Then to advance to the given child cell, we add
  // (2 * position * new_lsb).
  uint64_t new_lsb = lsb() >> 2;
  return S2CellId(id_ + (2 * position + 1 - 4) * new_lsb);
}

inline S2CellId S2CellId::child_begin() const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK(!is_leaf());
  uint64_t old_lsb = lsb();
  return S2CellId(id_ - old_lsb + (old_lsb >> 2));
}

inline S2CellId S2CellId::child_begin(int level) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK_GE(level, this->level());
  ABSL_DCHECK_LE(level, kMaxLevel);
  return S2CellId(id_ - lsb() + lsb_for_level(level));
}

inline S2CellId S2CellId::child_end() const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK(!is_leaf());
  uint64_t old_lsb = lsb();
  return S2CellId(id_ + old_lsb + (old_lsb >> 2));
}

inline S2CellId S2CellId::child_end(int level) const {
  ABSL_DCHECK(is_valid());
  ABSL_DCHECK_GE(level, this->level());
  ABSL_DCHECK_LE(level, kMaxLevel);
  return S2CellId(id_ + lsb() + lsb_for_level(level));
}

inline S2CellId S2CellId::next() const {
  return S2CellId(id_ + (lsb() << 1));
}

inline S2CellId S2CellId::prev() const {
  return S2CellId(id_ - (lsb() << 1));
}

inline S2CellId S2CellId::next_wrap() const {
  ABSL_DCHECK(is_valid());
  S2CellId n = next();
  if (n.id_ < kWrapOffset) return n;
  return S2CellId(n.id_ - kWrapOffset);
}

inline S2CellId S2CellId::prev_wrap() const {
  ABSL_DCHECK(is_valid());
  S2CellId p = prev();
  if (p.id_ < kWrapOffset) return p;
  return S2CellId(p.id_ + kWrapOffset);
}

inline S2CellId S2CellId::Begin(int level) {
  // `child_begin` `ABSL_DCHECK`s level, so don't do it here.
  return FromFace(0).child_begin(level);
}

inline S2CellId S2CellId::End(int level) {
  // `child_end` `ABSL_DCHECK`s level, so don't do it here.
  return FromFace(5).child_end(level);
}

std::ostream& operator<<(std::ostream& os, S2CellId id);

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

// Parse valid S2 tokens from a string. Returns false if the token cannot be
// parsed with S2CellId::FromToken.
bool AbslParseFlag(absl::string_view input, S2CellId* id, std::string* error);

// Unparse a valid S2 token into a string that can be parsed by AbslParseFlag.
std::string AbslUnparseFlag(S2CellId id);

#undef IFNDEF_SWIG
}
#endif  // S2_S2CELL_ID_V1_H_
