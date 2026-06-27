#include "world/world_gen.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <vector>

#include "rng/rng.h"

namespace pyrelite
{
    namespace
    {
        // kZoneChunks and kZoneSize are public (world/zone.h).
        constexpr int kZoneLast = kZoneSize - 1;
        constexpr int kZoneCells = kZoneSize * kZoneSize;
        constexpr int kProvinceZones = 2;
        constexpr int kSpawnSealInnerRadius = 6;
        constexpr int kSpawnSealOuterRadius = 13;

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

        // The difficulty/theme tier of a whole generation zone, rising with the zone's
        // Chebyshev distance from the origin. This is THE escalation policy: both the
        // generated style (styleFor) and the view palette (worldTier) read it, so they
        // can never diverge. Change the cadence here and the whole world follows.
        int tierForZone(int zoneX, int zoneY)
        {
            return std::min(std::max(std::abs(zoneX), std::abs(zoneY)), kMaxTier);
        }

        struct Point
        {
            int x;
            int y;

            bool operator==(const Point &) const = default;
        };

        constexpr Point kSpawn{kZoneSize / 2 + 1, kZoneSize / 2 + 1};

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
                (value >> 16) % 3 == 0 ? 1 : 0,
            };
        }

        struct StyleParams
        {
            int minCorridorRadius;
            int maxCorridorRadius;
            // The backbone (spanning tree) of a zone is carved wide so main routes read
            // as arteries, while loop chords stay at the narrow corridor radius. This is
            // what breaks the uniform "thin maze" look without opening the whole zone.
            int arteryRadius;
            int bankDepth;
            int brickPct;
            int minRooms;
            int maxRooms;
            int islandPct;
        };

        StyleParams tierBase(int tier)
        {
            // islandPct (last field) is deliberately low: scattered single bricks in open
            // floor read as clutter, so the world stays calmer with only a light dusting.
            switch (tier)
            {
            case 0:  return {1, 1, 2, 3, 30, 4, 7, 2};
            case 1:  return {1, 1, 2, 3, 28, 4, 7, 2};
            case 2:  return {1, 1, 2, 3, 25, 4, 7, 3};
            case 3:  return {1, 1, 2, 2, 22, 4, 6, 3};
            default: return {1, 1, 2, 2, 20, 3, 6, 4};
            }
        }

        StyleParams styleFor(Biome biome, int tier)
        {
            StyleParams style = tierBase(tier);
            switch (biome)
            {
            case Biome::Hall:
                style.minRooms = std::max(3, style.minRooms - 1);
                style.maxRooms = std::max(style.minRooms, style.maxRooms - 2);
                style.maxCorridorRadius += 1;
                style.arteryRadius += 1;
                break;
            case Biome::Warren:
                style.minRooms += 2;
                style.maxRooms += 3;
                break;
            case Biome::Pillars:
                style.islandPct += 7;
                break;
            case Biome::Thicket:
                style.brickPct += 22;
                style.islandPct += 3;
                break;
            case Biome::Cavern:
                style.minRooms += 1;
                style.maxRooms += 2;
                style.maxCorridorRadius += 1;
                style.arteryRadius += 1;
                break;
            }
            return style;
        }

        void carveCell(Zone &zone, int x, int y)
        {
            if (x >= 1 && x < kZoneLast && y >= 1 && y < kZoneLast)
                zone.set(x, y, Tile::Empty);
        }

        void carveDisc(Zone &zone, int centerX, int centerY, int radius)
        {
            const int radiusSquared = radius * radius;
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx)
                    if (dx * dx + dy * dy <= radiusSquared)
                        carveCell(zone, centerX + dx, centerY + dy);
        }

        void carveEllipse(Zone &zone, int centerX, int centerY,
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

        void carveCapsule(Zone &zone, Point start, Point end, int radius)
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

        struct SpawnLayout
        {
            Point exitAnchor;
        };

        SpawnLayout spawnLayout(std::uint64_t seed)
        {
            // Sixteen directions around the chamber. Integer endpoints avoid floating
            // point while still giving axial, diagonal and intermediate tunnel angles.
            constexpr std::array<Point, 16> offsets = {{
                {14, 0}, {13, 5}, {10, 10}, {5, 13},
                {0, 14}, {-5, 13}, {-10, 10}, {-13, 5},
                {-14, 0}, {-13, -5}, {-10, -10}, {-5, -13},
                {0, -14}, {5, -13}, {10, -10}, {13, -5},
            }};
            const Point offset = offsets[coordinateValue(seed, 0, 0,
                0xA0761D6478BD642FULL) % offsets.size()];
            return {{kSpawn.x + offset.x, kSpawn.y + offset.y}};
        }

        void carveSpawnChamber(Zone &zone, const SpawnLayout &layout)
        {
            carveEllipse(zone, kSpawn.x, kSpawn.y, 5, 4);
            carveDisc(zone, kSpawn.x - 2, kSpawn.y + 1, 3);
            carveCapsule(zone, kSpawn, layout.exitAnchor, 1);
        }

        void sealSpawnChamber(Zone &zone, const SpawnLayout &layout)
        {
            const int innerSquared = kSpawnSealInnerRadius * kSpawnSealInnerRadius;
            const int outerSquared = kSpawnSealOuterRadius * kSpawnSealOuterRadius;
            for (int y = kSpawn.y - kSpawnSealOuterRadius;
                 y <= kSpawn.y + kSpawnSealOuterRadius; ++y)
                for (int x = kSpawn.x - kSpawnSealOuterRadius;
                     x <= kSpawn.x + kSpawnSealOuterRadius; ++x)
                {
                    const int dx = x - kSpawn.x;
                    const int dy = y - kSpawn.y;
                    const int distanceSquared = dx * dx + dy * dy;
                    if (distanceSquared >= innerSquared && distanceSquared <= outerSquared)
                        zone.set(x, y, Tile::Wall);
                }

            // Reopen exactly the chamber and its single tunnel after any unrelated
            // corridor that crossed the protected ring has been sealed.
            carveSpawnChamber(zone, layout);
        }

        bool protectedSpawnWall(int x, int y)
        {
            const int dx = x - kSpawn.x;
            const int dy = y - kSpawn.y;
            const int distanceSquared = dx * dx + dy * dy;
            return distanceSquared >= kSpawnSealInnerRadius * kSpawnSealInnerRadius
                && distanceSquared <= kSpawnSealOuterRadius * kSpawnSealOuterRadius;
        }

        std::array<bool, kZoneCells> floorReachableFromSpawn(const Zone &zone)
        {
            std::array<bool, kZoneCells> reached{};
            std::vector<int> queue{kSpawn.y * kZoneSize + kSpawn.x};
            for (std::size_t head = 0; head < queue.size(); ++head)
            {
                const int index = queue[head];
                if (reached[static_cast<std::size_t>(index)])
                    continue;
                const int x = index % kZoneSize;
                const int y = index / kZoneSize;
                if (zone.at(x, y) != Tile::Empty)
                    continue;
                reached[static_cast<std::size_t>(index)] = true;
                if (x > 0)         queue.push_back(index - 1);
                if (x < kZoneLast) queue.push_back(index + 1);
                if (y > 0)         queue.push_back(index - kZoneSize);
                if (y < kZoneLast) queue.push_back(index + kZoneSize);
            }
            return reached;
        }

        // Sealing the chamber can cut a corridor that happened to cross its protected
        // ring. Repair such components through rock outside the ring before banks are
        // classified, preserving both global connectivity and the single entrance.
        void connectFloorOutsideSpawnRing(Zone &zone)
        {
            while (true)
            {
                const auto reached = floorReachableFromSpawn(zone);
                int target = -1;
                for (int index = 0; index < kZoneCells; ++index)
                    if (zone.at(index % kZoneSize, index / kZoneSize) == Tile::Empty
                        && !reached[static_cast<std::size_t>(index)])
                    {
                        target = index;
                        break;
                    }
                if (target < 0)
                    return;

                std::array<int, kZoneCells> previous;
                previous.fill(-1);
                std::vector<int> queue;
                queue.reserve(kZoneCells);
                for (int index = 0; index < kZoneCells; ++index)
                    if (reached[static_cast<std::size_t>(index)])
                    {
                        previous[static_cast<std::size_t>(index)] = -2;
                        queue.push_back(index);
                    }

                int connection = -1;
                for (std::size_t head = 0; head < queue.size() && connection < 0; ++head)
                {
                    const int index = queue[head];
                    const int x = index % kZoneSize;
                    const int y = index / kZoneSize;
                    const int neighbours[] = {
                        x > 0 ? index - 1 : -1,
                        x < kZoneLast ? index + 1 : -1,
                        y > 0 ? index - kZoneSize : -1,
                        y < kZoneLast ? index + kZoneSize : -1,
                    };
                    for (int neighbour : neighbours)
                    {
                        if (neighbour < 0
                            || previous[static_cast<std::size_t>(neighbour)] != -1)
                            continue;
                        const int nx = neighbour % kZoneSize;
                        const int ny = neighbour / kZoneSize;
                        const bool boundary = nx == 0 || nx == kZoneLast
                            || ny == 0 || ny == kZoneLast;
                        if (protectedSpawnWall(nx, ny) && zone.at(nx, ny) != Tile::Empty)
                            continue;
                        if (boundary && zone.at(nx, ny) != Tile::Empty)
                            continue;

                        previous[static_cast<std::size_t>(neighbour)] = index;
                        if (zone.at(nx, ny) == Tile::Empty
                            && !reached[static_cast<std::size_t>(neighbour)])
                        {
                            connection = neighbour;
                            break;
                        }
                        queue.push_back(neighbour);
                    }
                }

                if (connection < 0)
                    return;
                for (int index = connection;
                     previous[static_cast<std::size_t>(index)] >= 0;
                     index = previous[static_cast<std::size_t>(index)])
                    zone.set(index % kZoneSize, index / kZoneSize, Tile::Empty);
            }
        }

        void carvePortalVertical(Zone &zone, int edgeX, const Portal &portal)
        {
            const int innerX = edgeX == 0 ? 1 : kZoneLast - 1;
            for (int offset = -portal.halfWidth; offset <= portal.halfWidth; ++offset)
            {
                zone.set(edgeX, portal.center + offset, Tile::Empty);
                zone.set(innerX, portal.center + offset, Tile::Empty);
            }
        }

        void carvePortalHorizontal(Zone &zone, int edgeY, const Portal &portal)
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
        void carveCurvedCorridor(Zone &zone, Rng &rng, Point start, Point end,
                                 int radiusMin, int radiusMax)
        {
            const int dx = end.x - start.x;
            const int dy = end.y - start.y;
            const int distance = std::max(std::abs(dx), std::abs(dy));
            if (distance == 0)
            {
                carveDisc(zone, start.x, start.y, radiusMin);
                return;
            }

            const int maxBend = std::clamp(distance / 2, 4, 16);
            const int bend = randomBetween(rng, -maxBend, maxBend);

            Point control{(start.x + end.x) / 2, (start.y + end.y) / 2};
            control.x -= dy * bend / distance;
            control.y += dx * bend / distance;
            control.x = std::clamp(control.x, 3, kZoneLast - 3);
            control.y = std::clamp(control.y, 3, kZoneLast - 3);

            const int steps = std::max(1, distance * 2);
            const int denominator = steps * steps;
            int radius = randomBetween(rng, radiusMin, radiusMax);
            for (int step = 0; step <= steps; ++step)
            {
                if (step > 0 && step % 9 == 0)
                {
                    radius += randomBetween(rng, -1, 1);
                    radius = std::clamp(radius, radiusMin, radiusMax);
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

        void carveOrganicRoom(Zone &zone, Rng &rng, Point center, Biome biome)
        {
            const int shape = static_cast<int>(rng.below(3));
            if (shape == 0)
            {
                int radiusX = randomBetween(rng, 3, biome == Biome::Cavern ? 6 : 5);
                int radiusY = randomBetween(rng, 2, biome == Biome::Cavern ? 5 : 4);
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
                const int length = randomBetween(rng, 7, 13);
                const int radius = randomBetween(rng, 1, 2);
                for (int step = -length / 2; step <= length / 2; ++step)
                    carveDisc(zone, center.x + directionX * step,
                              center.y + directionY * step, radius);
                return;
            }

            const int baseRadius = randomBetween(rng, 2, biome == Biome::Cavern ? 4 : 3);
            carveDisc(zone, center.x, center.y, baseRadius);
            const int lobes = randomBetween(rng, 2, biome == Biome::Warren ? 5 : 4);
            for (int lobe = 0; lobe < lobes; ++lobe)
            {
                const Point lobeCenter{
                    center.x + randomBetween(rng, -5, 5),
                    center.y + randomBetween(rng, -5, 5),
                };
                const int lobeRadius = randomBetween(rng, 2, 3);
                carveCapsule(zone, center, lobeCenter, 1);
                carveDisc(zone, lobeCenter.x, lobeCenter.y, lobeRadius);
            }
        }

        void connectNodes(Zone &zone, Rng &rng, const std::vector<Point> &nodes,
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
                // The tree edges are the zone's backbone, so they are carved wide as
                // arteries; only the optional chords below stay at the narrow radius.
                carveCurvedCorridor(zone, rng, nodes[bestFrom], nodes[bestTo],
                                    style.arteryRadius - 1, style.arteryRadius);
                connected[bestTo] = true;
            }

            // Occasional chords turn the tree of rooms into local loops. They are not
            // needed for connectivity and therefore remain purely stylistic; narrow so
            // the backbone arteries stay the legible main routes.
            const int extraEdges = static_cast<int>(rng.below(2));
            for (int edge = 0; edge < extraEdges; ++edge)
            {
                const auto from = static_cast<std::size_t>(rng.below(
                    static_cast<std::uint32_t>(nodes.size())));
                const auto to = static_cast<std::size_t>(rng.below(
                    static_cast<std::uint32_t>(nodes.size())));
                if (from != to)
                    carveCurvedCorridor(zone, rng, nodes[from], nodes[to],
                                        style.minCorridorRadius, style.maxCorridorRadius);
            }
        }

        bool touchesFloor(const Zone &zone, int x, int y)
        {
            return (x > 0 && zone.at(x - 1, y) == Tile::Empty)
                || (x < kZoneLast && zone.at(x + 1, y) == Tile::Empty)
                || (y > 0 && zone.at(x, y - 1) == Tile::Empty)
                || (y < kZoneLast && zone.at(x, y + 1) == Tile::Empty);
        }

        // A 3/4 chamfer distance is a cheap integer approximation of Euclidean distance.
        // Unlike the old Manhattan flood it follows rounded chambers without producing
        // conspicuous diamond banks.
        void applyBanks(Zone &zone, Rng &rng, const StyleParams &style)
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

        bool openThreeByThree(const Zone &zone, int centerX, int centerY)
        {
            for (int y = centerY - 1; y <= centerY + 1; ++y)
                for (int x = centerX - 1; x <= centerX + 1; ++x)
                    if (zone.at(x, y) != Tile::Empty)
                        return false;
            return true;
        }

        bool brickNearby(const Zone &zone, int centerX, int centerY)
        {
            for (int y = centerY - 2; y <= centerY + 2; ++y)
                for (int x = centerX - 2; x <= centerX + 2; ++x)
                    if (zone.at(x, y) == Tile::Brick)
                        return true;
            return false;
        }

        void placeFloorIslands(Zone &zone, std::uint64_t seed,
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
                        && std::abs(globalX - 1) <= kSpawnSealOuterRadius + 1
                        && std::abs(globalY - 1) <= kSpawnSealOuterRadius + 1)
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

        // A large open chamber carved out of a zone. Unlike the small organic rooms it
        // is then packed with a destructible BRICK pattern, so the player meets a roomy
        // arena full of things to blow up (and cover to fight behind) rather than empty
        // floor. Recorded by ellipse so the fill pass can address exactly its footprint.
        struct Arena
        {
            Point center;
            int radiusX;
            int radiusY;
        };

        // The brick layouts an arena can be packed with. Noise stays in the catalog so
        // some arenas still read as organic rubble; the rest are recognisable shapes that
        // make a chamber a landmark instead of yet another patch of random bricks.
        enum class BrickPattern { Noise, Lattice, Rings, Cross, Border, Clusters, Pillars };

        bool inEllipse(Point center, int radiusX, int radiusY, int x, int y)
        {
            const long long dx = x - center.x;
            const long long dy = y - center.y;
            const long long rx2 = static_cast<long long>(radiusX) * radiusX;
            const long long ry2 = static_cast<long long>(radiusY) * radiusY;
            return dx * dx * ry2 + dy * dy * rx2 <= rx2 * ry2;
        }

        // A footprint cell on the ellipse boundary: one whose four-neighbourhood leaves
        // the footprint. The fill keeps the rim Empty so the arena always stays walkable
        // around its edge and through-traffic between its corridors is preserved.
        bool isArenaRim(const Arena &arena, int x, int y)
        {
            return !inEllipse(arena.center, arena.radiusX, arena.radiusY, x - 1, y)
                || !inEllipse(arena.center, arena.radiusX, arena.radiusY, x + 1, y)
                || !inEllipse(arena.center, arena.radiusX, arena.radiusY, x, y - 1)
                || !inEllipse(arena.center, arena.radiusX, arena.radiusY, x, y + 1);
        }

        // Biome-weighted pattern choice: each biome favours layouts that reinforce its
        // character (Pillars gets pillar fields, Thicket gets dense rubble, Hall gets
        // grand rings, ...), so biomes finally differ in SHAPE, not only in numbers.
        BrickPattern choosePattern(Biome biome, std::uint64_t value)
        {
            switch (biome)
            {
            case Biome::Hall:
            {
                static constexpr BrickPattern c[] = {
                    BrickPattern::Rings, BrickPattern::Border,
                    BrickPattern::Cross, BrickPattern::Noise};
                return c[value % std::size(c)];
            }
            case Biome::Warren:
            {
                static constexpr BrickPattern c[] = {
                    BrickPattern::Clusters, BrickPattern::Lattice, BrickPattern::Noise};
                return c[value % std::size(c)];
            }
            case Biome::Pillars:
            {
                static constexpr BrickPattern c[] = {
                    BrickPattern::Pillars, BrickPattern::Lattice, BrickPattern::Cross};
                return c[value % std::size(c)];
            }
            case Biome::Thicket:
            {
                static constexpr BrickPattern c[] = {
                    BrickPattern::Noise, BrickPattern::Clusters, BrickPattern::Lattice};
                return c[value % std::size(c)];
            }
            case Biome::Cavern:
            {
                static constexpr BrickPattern c[] = {
                    BrickPattern::Noise, BrickPattern::Rings, BrickPattern::Border};
                return c[value % std::size(c)];
            }
            }
            return BrickPattern::Noise;
        }

        int arenaNoiseDensity(Biome biome)
        {
            switch (biome)
            {
            case Biome::Thicket: return 46;
            case Biome::Warren:  return 38;
            case Biome::Cavern:  return 32;
            case Biome::Hall:    return 28;
            case Biome::Pillars: return 30;
            }
            return 34;
        }

        // Packing an arena with brick can seal it off: its connecting artery is bricked
        // over, or a pattern leaves an enclosed Empty pocket. Restore a single connected
        // floor by carving the shortest path from the main component to each stranded one
        // — but only through cells the fill itself could have produced, never the zone
        // boundary, the origin spawn ring, Void, or any cell touching Void. Every brick
        // the fill placed sat on former Empty floor, which the prior stages guarantee is
        // never adjacent to Void, so this can reconnect a sealed arena without ever
        // exposing the abyss or opening a second spawn exit.
        void reconnectFloorThroughFill(Zone &zone, bool isOrigin)
        {
            auto touchesVoid = [&](int x, int y)
            {
                return (x > 0 && zone.at(x - 1, y) == Tile::Void)
                    || (x < kZoneLast && zone.at(x + 1, y) == Tile::Void)
                    || (y > 0 && zone.at(x, y - 1) == Tile::Void)
                    || (y < kZoneLast && zone.at(x, y + 1) == Tile::Void);
            };

            while (true)
            {
                int seed = -1;
                for (int index = 0; index < kZoneCells && seed < 0; ++index)
                    if (zone.at(index % kZoneSize, index / kZoneSize) == Tile::Empty)
                        seed = index;
                if (seed < 0)
                    return;

                std::array<bool, kZoneCells> reached{};
                std::vector<int> flood{seed};
                for (std::size_t head = 0; head < flood.size(); ++head)
                {
                    const int index = flood[head];
                    if (reached[static_cast<std::size_t>(index)])
                        continue;
                    const int x = index % kZoneSize;
                    const int y = index / kZoneSize;
                    if (zone.at(x, y) != Tile::Empty)
                        continue;
                    reached[static_cast<std::size_t>(index)] = true;
                    if (x > 0)         flood.push_back(index - 1);
                    if (x < kZoneLast) flood.push_back(index + 1);
                    if (y > 0)         flood.push_back(index - kZoneSize);
                    if (y < kZoneLast) flood.push_back(index + kZoneSize);
                }

                int target = -1;
                for (int index = 0; index < kZoneCells; ++index)
                    if (zone.at(index % kZoneSize, index / kZoneSize) == Tile::Empty
                        && !reached[static_cast<std::size_t>(index)])
                    {
                        target = index;
                        break;
                    }
                if (target < 0)
                    return;

                std::array<int, kZoneCells> previous;
                previous.fill(-1);
                std::vector<int> queue;
                queue.reserve(kZoneCells);
                for (int index = 0; index < kZoneCells; ++index)
                    if (reached[static_cast<std::size_t>(index)])
                    {
                        previous[static_cast<std::size_t>(index)] = -2;
                        queue.push_back(index);
                    }

                int connection = -1;
                for (std::size_t head = 0; head < queue.size() && connection < 0; ++head)
                {
                    const int index = queue[head];
                    const int x = index % kZoneSize;
                    const int y = index / kZoneSize;
                    const int neighbours[] = {
                        x > 0 ? index - 1 : -1,
                        x < kZoneLast ? index + 1 : -1,
                        y > 0 ? index - kZoneSize : -1,
                        y < kZoneLast ? index + kZoneSize : -1,
                    };
                    for (int neighbour : neighbours)
                    {
                        if (neighbour < 0
                            || previous[static_cast<std::size_t>(neighbour)] != -1)
                            continue;
                        const int nx = neighbour % kZoneSize;
                        const int ny = neighbour / kZoneSize;
                        const bool empty = zone.at(nx, ny) == Tile::Empty;
                        if (!empty)
                        {
                            const bool boundary = nx == 0 || nx == kZoneLast
                                || ny == 0 || ny == kZoneLast;
                            if (boundary || zone.at(nx, ny) == Tile::Void
                                || touchesVoid(nx, ny)
                                || (isOrigin && protectedSpawnWall(nx, ny)))
                                continue;
                        }

                        previous[static_cast<std::size_t>(neighbour)] = index;
                        if (empty && !reached[static_cast<std::size_t>(neighbour)])
                        {
                            connection = neighbour;
                            break;
                        }
                        queue.push_back(neighbour);
                    }
                }

                if (connection < 0)
                    return;
                for (int index = connection;
                     previous[static_cast<std::size_t>(index)] >= 0;
                     index = previous[static_cast<std::size_t>(index)])
                    zone.set(index % kZoneSize, index / kZoneSize, Tile::Empty);
            }
        }

        void fillArena(Zone &zone, std::uint64_t seed, const Arena &arena, Biome biome)
        {
            const int rX = arena.radiusX;
            const int rY = arena.radiusY;
            const int cx = arena.center.x;
            const int cy = arena.center.y;
            const BrickPattern pattern = choosePattern(biome,
                coordinateValue(seed, cx, cy, 0x51A2C3D4E5F60718ULL));
            const int density = arenaNoiseDensity(biome);
            const int globalMinX = zone.zoneX() * kZoneSize - kZoneSize / 2;
            const int globalMinY = zone.zoneY() * kZoneSize - kZoneSize / 2;
            const long long rx2 = static_cast<long long>(rX) * rX;
            const long long ry2 = static_cast<long long>(rY) * rY;
            const long long maxR2 = rx2 * ry2;

            // Only ever convert open interior floor: keep the rim Empty and never touch a
            // cell that is not currently floor, so the fill stays inside its footprint.
            auto place = [&](int x, int y, Tile tile)
            {
                if (tile == Tile::Empty
                    || x < 1 || x >= kZoneLast || y < 1 || y >= kZoneLast
                    || !inEllipse(arena.center, rX, rY, x, y)
                    || zone.at(x, y) != Tile::Empty
                    || isArenaRim(arena, x, y))
                    return;
                zone.set(x, y, tile);
            };

            auto noiseBrick = [&](int x, int y, int pct)
            {
                return static_cast<int>(coordinateValue(seed, globalMinX + x,
                    globalMinY + y, 0x3243F6A8885A308DULL) % 100) < pct;
            };

            if (pattern == BrickPattern::Clusters)
            {
                Rng rng(coordinateValue(seed, cx, cy, 0x2B7E151628AED2A6ULL));
                const int blobs = 3 + static_cast<int>(rng.below(4));
                for (int blob = 0; blob < blobs; ++blob)
                {
                    const Point at{
                        cx + randomBetween(rng, -(rX - 2), rX - 2),
                        cy + randomBetween(rng, -(rY - 2), rY - 2)};
                    const int radius = randomBetween(rng, 1, 3);
                    for (int dy = -radius; dy <= radius; ++dy)
                        for (int dx = -radius; dx <= radius; ++dx)
                            if (dx * dx + dy * dy <= radius * radius)
                                place(at.x + dx, at.y + dy, Tile::Brick);
                }
                return;
            }

            for (int y = cy - rY; y <= cy + rY; ++y)
                for (int x = cx - rX; x <= cx + rX; ++x)
                {
                    const int dx = x - cx;
                    const int dy = y - cy;
                    const long long r2 = static_cast<long long>(dx) * dx * ry2
                        + static_cast<long long>(dy) * dy * rx2;
                    const int frac = maxR2 > 0
                        ? static_cast<int>(r2 * 100 / maxR2) : 0;
                    Tile tile = Tile::Empty;
                    switch (pattern)
                    {
                    case BrickPattern::Noise:
                        if (noiseBrick(x, y, density))
                            tile = Tile::Brick;
                        break;
                    case BrickPattern::Lattice:
                        if ((dx & 1) == 0 && (dy & 1) == 0)
                            tile = Tile::Brick;
                        break;
                    case BrickPattern::Rings:
                        if ((frac / 22) % 2 == 0 && !(dy == 0 && dx > 0))
                            tile = Tile::Brick;
                        break;
                    case BrickPattern::Cross:
                        if (std::abs(dx) <= 1 || std::abs(dy) <= 1)
                            tile = Tile::Brick;
                        break;
                    case BrickPattern::Border:
                        if (frac >= 55)
                            tile = Tile::Brick;
                        break;
                    case BrickPattern::Pillars:
                        if (dx % 4 == 0 && dy % 4 == 0)
                            tile = Tile::Wall;
                        else if (noiseBrick(x, y, 32))
                            tile = Tile::Brick;
                        break;
                    case BrickPattern::Clusters:
                        break;
                    }
                    place(x, y, tile);
                }
        }

        Zone buildZone(std::uint64_t seed, int zoneX, int zoneY)
        {
            const Biome biome = biomeForZone(seed, zoneX, zoneY);
            Zone zone(zoneX, zoneY, biome);
            Rng rng(coordinateValue(seed, zoneX, zoneY, 0x4F1BBCDCBFA54001ULL));
            const int tier = tierForZone(zoneX, zoneY);
            const StyleParams style = styleFor(biome, tier);
            const Point here{zoneX, zoneY};
            const bool isOrigin = zoneX == 0 && zoneY == 0;
            SpawnLayout spawn{};
            std::vector<Point> nodes;
            if (isOrigin)
            {
                spawn = spawnLayout(seed);
                nodes.push_back(spawn.exitAnchor);
            }

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

            std::vector<Point> roomCenters;

            // One or two large arenas anchor the zone: roomy, brick-packed chambers that
            // give the labyrinth memorable landmarks. They are placed first so the small
            // rooms keep their distance, and joined like any other node so an artery
            // leads into them.
            std::vector<Arena> arenas;
            const int arenaCount = 1 + static_cast<int>(rng.below(2));
            for (int arena = 0; arena < arenaCount; ++arena)
            {
                const int radiusX = randomBetween(rng, 7, 10);
                const int radiusY = randomBetween(rng, 6, 8);
                const int margin = std::max(radiusX, radiusY) + 2;
                Point center{0, 0};
                bool found = false;
                for (int attempt = 0; attempt < 40 && !found; ++attempt)
                {
                    center = {
                        randomBetween(rng, margin, kZoneLast - margin),
                        randomBetween(rng, margin, kZoneLast - margin),
                    };
                    const int spawnDx = center.x - kSpawn.x;
                    const int spawnDy = center.y - kSpawn.y;
                    // Keep the whole arena ellipse clear of the spawn seal ring so the
                    // fill never bricks an exit cell or opens a second chamber exit.
                    const int spawnClear =
                        kSpawnSealOuterRadius + std::max(radiusX, radiusY) + 2;
                    if (isOrigin
                        && spawnDx * spawnDx + spawnDy * spawnDy < spawnClear * spawnClear)
                        continue;

                    found = true;
                    for (const Point &other : roomCenters)
                    {
                        const int dx = center.x - other.x;
                        const int dy = center.y - other.y;
                        if (dx * dx + dy * dy < 16 * 16)
                        {
                            found = false;
                            break;
                        }
                    }
                }
                if (!found)
                    continue;
                carveEllipse(zone, center.x, center.y, radiusX, radiusY);
                arenas.push_back({center, radiusX, radiusY});
                roomCenters.push_back(center);
                nodes.push_back(center);
            }

            // Each arena is worth roughly two small rooms of open space, so trade them
            // off to keep the zone's open-floor budget (and the cave-not-field invariant).
            const int roomCount = std::max(2,
                randomBetween(rng, style.minRooms, style.maxRooms)
                    - 2 * static_cast<int>(arenas.size()));
            for (int room = 0; room < roomCount; ++room)
            {
                Point center{0, 0};
                bool found = false;
                for (int attempt = 0; attempt < 32 && !found; ++attempt)
                {
                    center = {
                        randomBetween(rng, 8, kZoneLast - 8),
                        randomBetween(rng, 8, kZoneLast - 8),
                    };
                    const int spawnDx = center.x - kSpawn.x;
                    const int spawnDy = center.y - kSpawn.y;
                    if (isOrigin && spawnDx * spawnDx + spawnDy * spawnDy < 18 * 18)
                        continue;

                    found = true;
                    for (const Point &other : roomCenters)
                    {
                        const int dx = center.x - other.x;
                        const int dy = center.y - other.y;
                        if (dx * dx + dy * dy < 12 * 12)
                        {
                            found = false;
                            break;
                        }
                    }
                }
                if (!found)
                    continue;
                carveOrganicRoom(zone, rng, center, biome);
                roomCenters.push_back(center);
                nodes.push_back(center);
            }

            // Small unadorned nodes become forks where several oblique tunnels can
            // meet without every branch swelling into another large room. Kept few: a
            // dense web of junctions is what made the labyrinth read as chaotic.
            const int junctionCount = 2 + static_cast<int>(rng.below(2));
            for (int junction = 0; junction < junctionCount; ++junction)
            {
                Point point{0, 0};
                bool found = false;
                for (int attempt = 0; attempt < 24 && !found; ++attempt)
                {
                    point = {
                        randomBetween(rng, 7, kZoneLast - 7),
                        randomBetween(rng, 7, kZoneLast - 7),
                    };
                    const int dx = point.x - kSpawn.x;
                    const int dy = point.y - kSpawn.y;
                    found = !isOrigin || dx * dx + dy * dy >= 17 * 17;
                }
                if (found)
                {
                    carveDisc(zone, point.x, point.y, 1);
                    nodes.push_back(point);
                }
            }

            if (isOrigin)
                carveSpawnChamber(zone, spawn);
            connectNodes(zone, rng, nodes, style);
            if (isOrigin)
            {
                sealSpawnChamber(zone, spawn);
                connectFloorOutsideSpawnRing(zone);
            }
            applyBanks(zone, rng, style);
            if (isOrigin)
                sealSpawnChamber(zone, spawn);
            for (const Arena &arena : arenas)
                fillArena(zone, seed, arena, biome);
            reconnectFloorThroughFill(zone, isOrigin);
            placeFloorIslands(zone, seed, style);
            return zone;
        }
    }

    Zone generateZone(std::uint64_t seed, int zoneX, int zoneY)
    {
        return buildZone(seed, zoneX, zoneY);
    }

    int worldTier(int chunkX, int chunkY)
    {
        return tierForZone(Zone::ofChunk(chunkX), Zone::ofChunk(chunkY));
    }

    // Pure per-chunk generation: build the containing zone and slice the chunk out. A
    // caller materializing many chunks of a zone should hold a ZoneCache so the zone is
    // built once instead of once per chunk.
    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY)
    {
        return generateZone(seed, Zone::ofChunk(chunkX), Zone::ofChunk(chunkY))
            .chunk(chunkX, chunkY);
    }
} // namespace pyrelite
