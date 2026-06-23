#include "world/world_gen.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#include "rng/rng.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kZoneChunks = 4;
        constexpr int kZoneSize = kZoneChunks * kChunkSize;
        constexpr int kZoneLast = kZoneSize - 1;
        constexpr int kZoneHalfChunks = kZoneChunks / 2;
        constexpr int kZoneCells = kZoneSize * kZoneSize;
        constexpr int kProvinceZones = 2;

        constexpr int kRingsPerTier = 2;
        constexpr int kMaxTier = 4;

        std::uint64_t mix64(std::uint64_t h)
        {
            h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
            h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
            return h ^ (h >> 31);
        }

        std::uint64_t coordinateValue(std::uint64_t seed, int x, int y,
                                      std::uint64_t salt)
        {
            std::uint64_t h = seed ^ salt;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(x))
                * 0xD1B54A32D192ED03ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(y))
                * 0x9E3779B97F4A7C15ULL;
            return mix64(h);
        }

        int floorDiv(int value, int divisor)
        {
            const int q = value / divisor;
            const int r = value % divisor;
            return r < 0 ? q - 1 : q;
        }

        int zoneOfChunk(int chunkCoord)
        {
            return floorDiv(chunkCoord + kZoneHalfChunks, kZoneChunks);
        }

        int zoneMinChunk(int zoneCoord)
        {
            return zoneCoord * kZoneChunks - kZoneHalfChunks;
        }

        struct Point
        {
            int x;
            int y;

            bool operator==(const Point &) const = default;
        };

        // Every non-origin zone chooses one cardinal parent one step closer to the
        // origin. These edges form an infinite rooted tree, guaranteeing that every
        // generated area belongs to the same world component. Other edges may be added
        // as loops, but never need to be opened merely because a chunk boundary exists.
        Point parentZone(std::uint64_t seed, int zoneX, int zoneY)
        {
            if (zoneX == 0 && zoneY == 0)
                return {0, 0};
            if (zoneX == 0)
                return {0, zoneY + (zoneY > 0 ? -1 : 1)};
            if (zoneY == 0)
                return {zoneX + (zoneX > 0 ? -1 : 1), 0};

            const bool moveX = coordinateValue(seed, zoneX, zoneY,
                                                0xFA9B6C83D4E1A257ULL) % 2 == 0;
            if (moveX)
                return {zoneX + (zoneX > 0 ? -1 : 1), zoneY};
            return {zoneX, zoneY + (zoneY > 0 ? -1 : 1)};
        }

        std::uint64_t edgeValue(std::uint64_t seed, Point a, Point b,
                                std::uint64_t salt)
        {
            if (a.x != b.x)
            {
                const int boundaryX = std::max(a.x, b.x);
                return coordinateValue(seed, boundaryX, a.y,
                                       salt ^ 0xA24BAED4963EE407ULL);
            }
            const int boundaryY = std::max(a.y, b.y);
            return coordinateValue(seed, a.x, boundaryY,
                                   salt ^ 0x9FB21C651E98DF25ULL);
        }

        bool zonesConnected(std::uint64_t seed, Point a, Point b)
        {
            if (parentZone(seed, a.x, a.y) == b || parentZone(seed, b.x, b.y) == a)
                return true;

            // Sparse non-tree edges make loops and alternate routes without restoring
            // the old mandatory four-way cross in every generation unit.
            return edgeValue(seed, a, b, 0xC6BC279692B5CC83ULL) % 100 < 18;
        }

        struct Portal
        {
            int center;
            int halfWidth;
        };

        Portal portalFor(std::uint64_t value)
        {
            constexpr int kMargin = 8;
            constexpr int kSpan = kZoneSize - 2 * kMargin;
            return {
                kMargin + static_cast<int>(value % kSpan),
                1 + static_cast<int>((value >> 16) % 3),
            };
        }

        struct StyleParams
        {
            int minCorridorRadius;
            int maxCorridorRadius;
            int bankDepth;
            int brickPct;
            int minRooms;
            int maxRooms;
            int islandPct;
        };

        StyleParams tierBase(int tier)
        {
            switch (tier)
            {
            case 0:  return {2, 4, 4, 30, 4, 7, 8};
            case 1:  return {2, 4, 3, 28, 4, 7, 9};
            case 2:  return {2, 3, 3, 25, 3, 6, 10};
            case 3:  return {1, 3, 2, 22, 3, 6, 11};
            default: return {1, 2, 2, 20, 3, 5, 12};
            }
        }

        StyleParams styleFor(Biome biome, int tier)
        {
            StyleParams style = tierBase(tier);
            switch (biome)
            {
            case Biome::Hall:
                style.minRooms = std::max(2, style.minRooms - 1);
                style.maxRooms = std::max(style.minRooms, style.maxRooms - 2);
                style.maxCorridorRadius += 1;
                break;
            case Biome::Warren:
                style.minRooms += 2;
                style.maxRooms += 4;
                break;
            case Biome::Pillars:
                style.islandPct += 14;
                break;
            case Biome::Thicket:
                style.brickPct += 22;
                style.islandPct += 5;
                break;
            case Biome::Cavern:
                style.minRooms += 1;
                style.maxRooms += 2;
                style.minCorridorRadius = std::max(style.minCorridorRadius, 2);
                break;
            }
            return style;
        }

        class GeneratedZone
        {
        public:
            GeneratedZone(int zoneX, int zoneY, Biome biome)
                : m_zoneX(zoneX)
                , m_zoneY(zoneY)
                , m_biome(biome)
            {
                m_tiles.fill(Tile::Wall);
            }

            int zoneX() const { return m_zoneX; }
            int zoneY() const { return m_zoneY; }
            Biome biome() const { return m_biome; }

            Tile at(int x, int y) const
            {
                return m_tiles[static_cast<std::size_t>(y) * kZoneSize + x];
            }

            void set(int x, int y, Tile tile)
            {
                m_tiles[static_cast<std::size_t>(y) * kZoneSize + x] = tile;
            }

        private:
            int m_zoneX;
            int m_zoneY;
            Biome m_biome;
            std::array<Tile, kZoneCells> m_tiles;
        };

        void carveCell(GeneratedZone &zone, int x, int y)
        {
            if (x >= 1 && x < kZoneLast && y >= 1 && y < kZoneLast)
                zone.set(x, y, Tile::Empty);
        }

        void carveDisc(GeneratedZone &zone, int centerX, int centerY, int radius)
        {
            const int radiusSquared = radius * radius;
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx)
                    if (dx * dx + dy * dy <= radiusSquared)
                        carveCell(zone, centerX + dx, centerY + dy);
        }

        void carveEllipse(GeneratedZone &zone, int centerX, int centerY,
                          int radiusX, int radiusY)
        {
            const int radiusXSquared = radiusX * radiusX;
            const int radiusYSquared = radiusY * radiusY;
            const int limit = radiusXSquared * radiusYSquared;
            for (int dy = -radiusY; dy <= radiusY; ++dy)
                for (int dx = -radiusX; dx <= radiusX; ++dx)
                    if (dx * dx * radiusYSquared + dy * dy * radiusXSquared <= limit)
                        carveCell(zone, centerX + dx, centerY + dy);
        }

        void carveCapsule(GeneratedZone &zone, Point start, Point end, int radius)
        {
            const int steps = std::max(std::abs(end.x - start.x), std::abs(end.y - start.y));
            if (steps == 0)
            {
                carveDisc(zone, start.x, start.y, radius);
                return;
            }
            for (int step = 0; step <= steps; ++step)
            {
                const int x = (start.x * (steps - step) + end.x * step + steps / 2) / steps;
                const int y = (start.y * (steps - step) + end.y * step + steps / 2) / steps;
                carveDisc(zone, x, y, radius);
            }
        }

        void carvePortalVertical(GeneratedZone &zone, int edgeX, const Portal &portal)
        {
            const int innerX = edgeX == 0 ? 1 : kZoneLast - 1;
            for (int offset = -portal.halfWidth; offset <= portal.halfWidth; ++offset)
            {
                zone.set(edgeX, portal.center + offset, Tile::Empty);
                zone.set(innerX, portal.center + offset, Tile::Empty);
            }
        }

        void carvePortalHorizontal(GeneratedZone &zone, int edgeY, const Portal &portal)
        {
            const int innerY = edgeY == 0 ? 1 : kZoneLast - 1;
            for (int offset = -portal.halfWidth; offset <= portal.halfWidth; ++offset)
            {
                zone.set(portal.center + offset, edgeY, Tile::Empty);
                zone.set(portal.center + offset, innerY, Tile::Empty);
            }
        }

        int randomBetween(Rng &rng, int minimum, int maximum)
        {
            return minimum + static_cast<int>(rng.below(
                static_cast<std::uint32_t>(maximum - minimum + 1)));
        }

        // Integer quadratic Bezier rasterisation produces a smooth bend while keeping
        // seeded worlds bit-identical across platforms. Circular stamps hide the tile
        // staircase and let the passage breathe between narrow and broad stretches.
        void carveCurvedCorridor(GeneratedZone &zone, Rng &rng, Point start, Point end,
                                 const StyleParams &style)
        {
            const int dx = end.x - start.x;
            const int dy = end.y - start.y;
            const int distance = std::max(std::abs(dx), std::abs(dy));
            const int maxBend = std::clamp(distance / 3, 3, 12);
            const int bend = randomBetween(rng, -maxBend, maxBend);

            Point control{(start.x + end.x) / 2, (start.y + end.y) / 2};
            if (std::abs(dx) >= std::abs(dy))
                control.y += bend;
            else
                control.x += bend;
            control.x = std::clamp(control.x, 3, kZoneLast - 3);
            control.y = std::clamp(control.y, 3, kZoneLast - 3);

            const int steps = std::max(1, distance * 2);
            const int denominator = steps * steps;
            int radius = randomBetween(rng, style.minCorridorRadius,
                                       style.maxCorridorRadius);
            for (int step = 0; step <= steps; ++step)
            {
                if (step > 0 && step % 9 == 0)
                {
                    radius += randomBetween(rng, -1, 1);
                    radius = std::clamp(radius, style.minCorridorRadius,
                                        style.maxCorridorRadius);
                }

                const int inverse = steps - step;
                const int xNumerator = inverse * inverse * start.x
                    + 2 * inverse * step * control.x + step * step * end.x;
                const int yNumerator = inverse * inverse * start.y
                    + 2 * inverse * step * control.y + step * step * end.y;
                const int x = (xNumerator + denominator / 2) / denominator;
                const int y = (yNumerator + denominator / 2) / denominator;
                carveDisc(zone, x, y, radius);
            }
        }

        void carveOrganicRoom(GeneratedZone &zone, Rng &rng, Point center, Biome biome)
        {
            const int shape = static_cast<int>(rng.below(3));
            if (shape == 0)
            {
                int radiusX = randomBetween(rng, 4, biome == Biome::Cavern ? 8 : 6);
                int radiusY = randomBetween(rng, 3, biome == Biome::Cavern ? 7 : 6);
                if (rng.chance(50))
                    std::swap(radiusX, radiusY);
                carveEllipse(zone, center.x, center.y, radiusX, radiusY);
                return;
            }

            if (shape == 1)
            {
                const int directionX = randomBetween(rng, -1, 1);
                int directionY = randomBetween(rng, -1, 1);
                if (directionX == 0 && directionY == 0)
                    directionY = 1;
                const int length = randomBetween(rng, 8, 16);
                const int radius = randomBetween(rng, 2, biome == Biome::Hall ? 4 : 3);
                for (int step = -length / 2; step <= length / 2; ++step)
                    carveDisc(zone, center.x + directionX * step,
                              center.y + directionY * step, radius);
                return;
            }

            const int baseRadius = randomBetween(rng, 3, biome == Biome::Cavern ? 6 : 5);
            carveDisc(zone, center.x, center.y, baseRadius);
            const int lobes = randomBetween(rng, 3, biome == Biome::Warren ? 7 : 5);
            for (int lobe = 0; lobe < lobes; ++lobe)
            {
                const Point lobeCenter{
                    center.x + randomBetween(rng, -7, 7),
                    center.y + randomBetween(rng, -7, 7),
                };
                const int lobeRadius = randomBetween(rng, 2, 5);
                carveCapsule(zone, center, lobeCenter, std::min(2, lobeRadius));
                carveDisc(zone, lobeCenter.x, lobeCenter.y, lobeRadius);
            }
        }

        void connectNodes(GeneratedZone &zone, Rng &rng, const std::vector<Point> &nodes,
                          const StyleParams &style)
        {
            if (nodes.size() < 2)
                return;

            std::vector<bool> connected(nodes.size(), false);
            connected[0] = true;
            for (std::size_t edge = 1; edge < nodes.size(); ++edge)
            {
                int bestDistance = std::numeric_limits<int>::max();
                std::size_t bestFrom = 0;
                std::size_t bestTo = 0;
                for (std::size_t from = 0; from < nodes.size(); ++from)
                {
                    if (!connected[from])
                        continue;
                    for (std::size_t to = 0; to < nodes.size(); ++to)
                    {
                        if (connected[to])
                            continue;
                        const int dx = nodes[to].x - nodes[from].x;
                        const int dy = nodes[to].y - nodes[from].y;
                        const int distance = dx * dx + dy * dy;
                        if (distance < bestDistance)
                        {
                            bestDistance = distance;
                            bestFrom = from;
                            bestTo = to;
                        }
                    }
                }
                carveCurvedCorridor(zone, rng, nodes[bestFrom], nodes[bestTo], style);
                connected[bestTo] = true;
            }

            // Occasional chords turn the tree of rooms into local loops. They are not
            // needed for connectivity and therefore remain purely stylistic.
            const int extraEdges = static_cast<int>(rng.below(3));
            for (int edge = 0; edge < extraEdges; ++edge)
            {
                const auto from = static_cast<std::size_t>(rng.below(
                    static_cast<std::uint32_t>(nodes.size())));
                const auto to = static_cast<std::size_t>(rng.below(
                    static_cast<std::uint32_t>(nodes.size())));
                if (from != to)
                    carveCurvedCorridor(zone, rng, nodes[from], nodes[to], style);
            }
        }

        bool touchesFloor(const GeneratedZone &zone, int x, int y)
        {
            return (x > 0 && zone.at(x - 1, y) == Tile::Empty)
                || (x < kZoneLast && zone.at(x + 1, y) == Tile::Empty)
                || (y > 0 && zone.at(x, y - 1) == Tile::Empty)
                || (y < kZoneLast && zone.at(x, y + 1) == Tile::Empty);
        }

        // A 3/4 chamfer distance is a cheap integer approximation of Euclidean distance.
        // Unlike the old Manhattan flood it follows rounded chambers without producing
        // conspicuous diamond banks.
        void applyBanks(GeneratedZone &zone, Rng &rng, const StyleParams &style)
        {
            constexpr int kFar = 1'000'000;
            std::array<int, kZoneCells> distance;
            for (int y = 0; y < kZoneSize; ++y)
                for (int x = 0; x < kZoneSize; ++x)
                    distance[static_cast<std::size_t>(y) * kZoneSize + x]
                        = zone.at(x, y) == Tile::Empty ? 0 : kFar;

            auto relax = [&distance](int x, int y, int nx, int ny, int cost)
            {
                if (nx < 0 || nx >= kZoneSize || ny < 0 || ny >= kZoneSize)
                    return;
                const auto index = static_cast<std::size_t>(y) * kZoneSize + x;
                const auto neighbour = static_cast<std::size_t>(ny) * kZoneSize + nx;
                distance[index] = std::min(distance[index], distance[neighbour] + cost);
            };

            for (int y = 0; y < kZoneSize; ++y)
                for (int x = 0; x < kZoneSize; ++x)
                {
                    relax(x, y, x - 1, y, 3);
                    relax(x, y, x, y - 1, 3);
                    relax(x, y, x - 1, y - 1, 4);
                    relax(x, y, x + 1, y - 1, 4);
                }
            for (int y = kZoneLast; y >= 0; --y)
                for (int x = kZoneLast; x >= 0; --x)
                {
                    relax(x, y, x + 1, y, 3);
                    relax(x, y, x, y + 1, 3);
                    relax(x, y, x + 1, y + 1, 4);
                    relax(x, y, x - 1, y + 1, 4);
                }

            const int bankLimit = style.bankDepth * 3;
            for (int y = 0; y < kZoneSize; ++y)
                for (int x = 0; x < kZoneSize; ++x)
                {
                    if (zone.at(x, y) == Tile::Empty)
                        continue;
                    const int d = distance[static_cast<std::size_t>(y) * kZoneSize + x];
                    if (d <= 3 && touchesFloor(zone, x, y))
                        zone.set(x, y, rng.chance(style.brickPct) ? Tile::Brick : Tile::Wall);
                    else if (d <= bankLimit)
                        zone.set(x, y, Tile::Wall);
                    else
                        zone.set(x, y, Tile::Void);
                }
        }

        bool openThreeByThree(const GeneratedZone &zone, int centerX, int centerY)
        {
            for (int y = centerY - 1; y <= centerY + 1; ++y)
                for (int x = centerX - 1; x <= centerX + 1; ++x)
                    if (zone.at(x, y) != Tile::Empty)
                        return false;
            return true;
        }

        bool brickNearby(const GeneratedZone &zone, int centerX, int centerY)
        {
            for (int y = centerY - 2; y <= centerY + 2; ++y)
                for (int x = centerX - 2; x <= centerX + 2; ++x)
                    if (zone.at(x, y) == Tile::Brick)
                        return true;
            return false;
        }

        void placeFloorIslands(GeneratedZone &zone, std::uint64_t seed,
                               const StyleParams &style)
        {
            const int globalMinX = zone.zoneX() * kZoneSize - kZoneSize / 2;
            const int globalMinY = zone.zoneY() * kZoneSize - kZoneSize / 2;
            for (int y = 3; y < kZoneLast - 2; ++y)
                for (int x = 3; x < kZoneLast - 2; ++x)
                {
                    const int globalX = globalMinX + x;
                    const int globalY = globalMinY + y;
                    if (zone.zoneX() == 0 && zone.zoneY() == 0
                        && std::abs(globalX - 1) <= 4 && std::abs(globalY - 1) <= 4)
                        continue;
                    if (coordinateValue(seed, globalX, globalY,
                                        0xDB4F0B9175AE2165ULL) % 100
                        >= static_cast<std::uint64_t>(style.islandPct))
                        continue;
                    if (openThreeByThree(zone, x, y) && !brickNearby(zone, x, y))
                        zone.set(x, y, Tile::Brick);
                }
        }

        Biome biomeForZone(std::uint64_t seed, int zoneX, int zoneY)
        {
            // One province spans several generation zones, so visual character persists
            // for hundreds of tiles instead of changing at every streamed chunk.
            const int provinceX = floorDiv(zoneX, kProvinceZones);
            const int provinceY = floorDiv(zoneY, kProvinceZones);
            return static_cast<Biome>(coordinateValue(seed, provinceX, provinceY,
                0x8CB92BA72F3D8DD7ULL) % kBiomeCount);
        }

        GeneratedZone generateZone(std::uint64_t seed, int zoneX, int zoneY)
        {
            const Biome biome = biomeForZone(seed, zoneX, zoneY);
            GeneratedZone zone(zoneX, zoneY, biome);
            Rng rng(coordinateValue(seed, zoneX, zoneY, 0x4F1BBCDCBFA54001ULL));
            const int tier = std::min(std::max(std::abs(zoneX), std::abs(zoneY)), kMaxTier);
            const StyleParams style = styleFor(biome, tier);
            const Point here{zoneX, zoneY};
            std::vector<Point> nodes;

            const Point west{zoneX - 1, zoneY};
            if (zonesConnected(seed, here, west))
            {
                const Portal portal = portalFor(edgeValue(seed, here, west,
                    0xE7037ED1A0B428DBULL));
                carvePortalVertical(zone, 0, portal);
                nodes.push_back({1, portal.center});
            }

            const Point east{zoneX + 1, zoneY};
            if (zonesConnected(seed, here, east))
            {
                const Portal portal = portalFor(edgeValue(seed, here, east,
                    0xE7037ED1A0B428DBULL));
                carvePortalVertical(zone, kZoneLast, portal);
                nodes.push_back({kZoneLast - 1, portal.center});
            }

            const Point north{zoneX, zoneY - 1};
            if (zonesConnected(seed, here, north))
            {
                const Portal portal = portalFor(edgeValue(seed, here, north,
                    0xE7037ED1A0B428DBULL));
                carvePortalHorizontal(zone, 0, portal);
                nodes.push_back({portal.center, 1});
            }

            const Point south{zoneX, zoneY + 1};
            if (zonesConnected(seed, here, south))
            {
                const Portal portal = portalFor(edgeValue(seed, here, south,
                    0xE7037ED1A0B428DBULL));
                carvePortalHorizontal(zone, kZoneLast, portal);
                nodes.push_back({portal.center, kZoneLast - 1});
            }

            const int roomCount = randomBetween(rng, style.minRooms, style.maxRooms);
            for (int room = 0; room < roomCount; ++room)
            {
                const Point center{
                    randomBetween(rng, 9, kZoneLast - 9),
                    randomBetween(rng, 9, kZoneLast - 9),
                };
                carveOrganicRoom(zone, rng, center, biome);
                nodes.push_back(center);
            }

            if (zoneX == 0 && zoneY == 0)
            {
                constexpr Point spawn{kZoneSize / 2 + 1, kZoneSize / 2 + 1};
                carveDisc(zone, spawn.x, spawn.y, 3);
                nodes.push_back(spawn);
            }

            connectNodes(zone, rng, nodes, style);
            applyBanks(zone, rng, style);
            placeFloorIslands(zone, seed, style);
            return zone;
        }
    }

    int worldTier(int chunkX, int chunkY)
    {
        const int ring = std::max(std::abs(chunkX), std::abs(chunkY));
        return std::min(ring / kRingsPerTier, kMaxTier);
    }

    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY)
    {
        const int zoneX = zoneOfChunk(chunkX);
        const int zoneY = zoneOfChunk(chunkY);
        const GeneratedZone zone = generateZone(seed, zoneX, zoneY);
        Chunk chunk(chunkX, chunkY, zone.biome());

        const int localChunkX = chunkX - zoneMinChunk(zoneX);
        const int localChunkY = chunkY - zoneMinChunk(zoneY);
        const int sourceX = localChunkX * kChunkSize;
        const int sourceY = localChunkY * kChunkSize;
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x)
                chunk.set(x, y, zone.at(sourceX + x, sourceY + y));
        return chunk;
    }
} // namespace pyrelite
