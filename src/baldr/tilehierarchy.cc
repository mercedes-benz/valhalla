#include "baldr/tilehierarchy.h"
#include "baldr/graphtileheader.h"
#include "midgard/vector2.h"

using namespace valhalla::midgard;

namespace valhalla {
namespace baldr {

// # level 0:  180
// # level 1:  90             rows: 2 cols: 4
// # level 2:  45             rows: 4 cols: 8
// # level 3:  22.5           rows: 8 cols 16
// # level 4:  11.25          rows: 16 cols 32
// # level 5:  5.625          rows: 32 cols 64
// # level 6:  2.8125         rows: 64 cols 128
// # level 7:  1.40625        rows: 128 cols 256
// # level 8:  0.703125       rows: 256 cols 512
// # level 9:  0.3515625      rows: 512 cols 1024
// # level 10: 0.17578125     rows: 1024 cols 2048    default data structures support size down to 0.125°
// # level 11: 0.087890625    rows: 2048 cols 4096
// # level 12: 0.0439453125   rows: 4096 cols 8192
// # level 13: 0.02197265625  rows: 8192 cols 16384

const std::vector<TileLevel>& TileHierarchy::levels() {
  // Static tile levels
  static const std::vector<TileLevel> levels_ = {

      TileLevel{0, stringToRoadClass("Primary"), "highway",
                midgard::Tiles<midgard::PointLL>{{-180, -90},
                                                 2.8125, //  NDS Level 6
                                                 128,
                                                 64,
                                                 static_cast<unsigned short>(kBinsDim)}},

      TileLevel{1, stringToRoadClass("Tertiary"), "arterial",
                midgard::Tiles<midgard::PointLL>{{-180, -90},
                                                 0.703125, //  NDS Level 8
                                                 512,
                                                 256,
                                                 static_cast<unsigned short>(kBinsDim)}},

      TileLevel{2, stringToRoadClass("ServiceOther"), "local",
                midgard::Tiles<midgard::PointLL>{{-180, -90},
                                                 0.17578125, //  NDS Level 10
                                                 2048,
                                                 1024,
                                                 static_cast<unsigned short>(kBinsDim)}},
  };

  return levels_;
}

const TileLevel& TileHierarchy::GetTransitLevel() {
  // Should we make a class lower than service other for transit?
  static const TileLevel transit_level_ =
      {3, stringToRoadClass("ServiceOther"), "transit",
       midgard::Tiles<midgard::PointLL>{{{-180, -90}, {180, 90}},
                                        .25,
                                        static_cast<unsigned short>(kBinsDim)}};

  return transit_level_;
}

midgard::AABB2<midgard::PointLL> TileHierarchy::GetGraphIdBoundingBox(const GraphId& id) {
  assert(id.Is_Valid());
  auto const& tileLevel = levels().at(id.level());
  return tileLevel.tiles.TileBounds(id.tileid());
}

// Returns the GraphId of the requested tile based on a lat,lng and a level.
// If the level is not supported an invalid id will be returned.
GraphId TileHierarchy::GetGraphId(const midgard::PointLL& pointll, const uint8_t level) {
  GraphId id;
  if (level < levels().size()) {
    auto tile_id = levels()[level].tiles.TileId(pointll);
    if (tile_id >= 0) {
      id = {static_cast<uint32_t>(tile_id), level, 0};
    }
  }
  return id;
}

// Gets the hierarchy level given the road class.
uint8_t TileHierarchy::get_level(const RoadClass roadclass) {
  if (roadclass <= levels()[0].importance) {
    return 0;
  } else if (roadclass <= levels()[1].importance) {
    return 1;
  } else {
    return 2;
  }
}

// Get the max hierarchy level.
uint8_t TileHierarchy::get_max_level() {
  return GetTransitLevel().level;
}

// Returns all the GraphIds of the tiles which intersect the given bounding
// box at that level.
std::vector<GraphId> TileHierarchy::GetGraphIds(const midgard::AABB2<midgard::PointLL>& bbox,
                                                const uint8_t level) {
  std::vector<GraphId> ids;
  if (level < levels().size()) {
    auto tile_ids = levels()[level].tiles.TileList(bbox);
    ids.reserve(tile_ids.size());

    for (auto tile_id : tile_ids) {
      ids.emplace_back(tile_id, level, 0);
    }
  }
  return ids;
}

// Returns all the GraphIds of the tiles which intersect the given bounding
// box at any level.
std::vector<GraphId> TileHierarchy::GetGraphIds(const midgard::AABB2<midgard::PointLL>& bbox) {
  std::vector<GraphId> ids;
  for (const auto& entry : levels()) {
    auto level_ids = GetGraphIds(bbox, entry.level);
    ids.reserve(ids.size() + level_ids.size());
    ids.insert(ids.end(), level_ids.begin(), level_ids.end());
  }
  return ids;
}

/**
 * Get the tiling system for a specified level.
 * @param level  Level Id.
 * @return Returns a const reference to the tiling system for this level.
 */
const midgard::Tiles<midgard::PointLL>& TileHierarchy::get_tiling(const uint8_t level) {
  const auto& transit_level = GetTransitLevel();
  if (level < levels().size()) {
    return levels()[level].tiles;
  } else if (level == transit_level.level) {
    return transit_level.tiles;
  }
  throw std::runtime_error("Invalid level Id for TileHierarchy::get_tiling");
}

GraphId TileHierarchy::parent(const GraphId& child_tile_id) {
  // bail if there is no parent
  if (child_tile_id.level() == 0)
    return GraphId(kInvalidGraphId);
  // get the tilings so we can use coordinates to pick the parent for the child
  auto parent_level = child_tile_id.level() - 1;
  const auto& parent_tiling = get_tiling(parent_level);
  const auto& child_tiling = get_tiling(child_tile_id.level());
  // grab just off of the childs corner to avoid edge cases
  auto corner = child_tiling.Base(child_tile_id.tileid()) +
                midgard::VectorXY<double>{parent_tiling.TileSize() / 2, parent_tiling.TileSize() / 2};
  // pick the parent from the childs coordinate
  auto parent_tile_index = parent_tiling.TileId(corner);
  return GraphId(parent_tile_index, parent_level, 0);
}

} // namespace baldr
} // namespace valhalla
