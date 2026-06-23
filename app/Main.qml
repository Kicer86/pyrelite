
import QtQuick
import QtQuick.Window
import PyreliteApp

Window {
    id: root

    property bool previewMode: false

    visibility: Qt.platform.os === "wasm" ? Window.FullScreen : Window.Windowed
    width: 960
    height: 820

    title: previewMode ? "Pyrelite — World Preview" : "Pyrelite"
    color: "#1b1b1b"

    BoardModel { id: board }

    // Tile size in pixels. Gameplay keeps the default scale; preview mode may change
    // it to sample a wider/narrower world window. Generation stays on a fixed chunk
    // radius around the camera, so a wide view can include unexplored space.
    readonly property real defaultCell: 48
    readonly property real minPreviewCell: 8
    readonly property real maxPreviewCell: 96
    property real cell: defaultCell
    readonly property int previewChunkTiles: 16
    readonly property color unknownTerrain: "#11151d"
    readonly property color unknownGrid: "#263142"

    // Region palette by world tier (board.tierAt): floor channel, rock bank, bombable
    // brick, and the void abyss seen behind the rock. Tiers rise with distance from
    // spawn, so the world visibly shifts to stranger, harsher hues the farther out the
    // player travels. The last entry is reused for any tier beyond it.
    readonly property var tierColors: [
        { floor: "#3a7d3a", rock: "#555a5e", brick: "#9c6b3c", abyss: "#0e1014" },
        { floor: "#4a7a3e", rock: "#6b4a3a", brick: "#b5703a", abyss: "#140d0b" },
        { floor: "#2f6f6a", rock: "#3f5a6e", brick: "#7a9bb0", abyss: "#07111a" },
        { floor: "#4a3f7a", rock: "#4a3a5e", brick: "#8a4aac", abyss: "#0b0814" },
        { floor: "#6a2f5a", rock: "#3a2a3e", brick: "#b03b7b", abyss: "#050309" }
    ]

    // Elastic camera follow. The player roams a central DEADZONE box without moving
    // the camera at all; only when they push past its edge does the camera scroll,
    // and it eases there (exponential catch-up) instead of snapping. This replaces
    // the Slice-A hard player-lock, which glued the world to the player every step
    // and felt stiff. Both knobs below are pure feel; view-only, core untouched.
    readonly property real deadzone: 0.4   // fraction of the viewport the box spans
    readonly property real followTau: 0.12 // catch-up time constant (s); larger = floatier
    readonly property real previewSpeed: 14 // tiles per second; intentionally faster than play

    property bool previewUp: false
    property bool previewDown: false
    property bool previewLeft: false
    property bool previewRight: false

    // Current camera offset, the source of truth: smoothed each frame (in the tick
    // below) toward cameraTarget. Initialised by recenter() once the view is sized.
    property real viewX: 0
    property real viewY: 0

    // Desired camera offset for one axis, given where the camera is NOW: keep the
    // player inside the central deadzone box, pushing the camera only when they cross
    // its edge. The world is infinite, so there is no border to clamp against.
    function cameraTarget(offset, viewport, playerTile) {
        const screen = (playerTile + 0.5) * cell + offset // player centre on screen
        const half = viewport * deadzone / 2
        if (screen < viewport / 2 - half)
            return offset + (viewport / 2 - half) - screen
        if (screen > viewport / 2 + half)
            return offset - (screen - (viewport / 2 + half))
        return offset
    }

    // Centred-on-player offset for one axis. Used only to snap the camera on load and
    // restart so it doesn't glide in from a stale position.
    function centeredOffset(viewport, playerTile) {
        return viewport / 2 - (playerTile + 0.5) * cell
    }

    // Snap the camera straight to the player (no easing), for load and restart.
    function recenter() {
        viewX = centeredOffset(scene.width, board.playerX)
        viewY = centeredOffset(scene.height, board.playerY)
    }

    function zoomPreview(factor) {
        const oldCell = cell
        const nextCell = Math.max(minPreviewCell, Math.min(maxPreviewCell, oldCell * factor))
        if (Math.abs(nextCell - oldCell) < 0.001)
            return

        // Preserve the world coordinate under the centre of the viewport. Changing
        // cell then makes the terrain repeater request a larger/smaller real tile area.
        const centerWorldX = (scene.width / 2 - viewX) / oldCell
        const centerWorldY = (scene.height / 2 - viewY) / oldCell
        cell = nextCell
        viewX = scene.width / 2 - centerWorldX * cell
        viewY = scene.height / 2 - centerWorldY * cell
    }

    function setPreviewKey(key, pressed) {
        switch (key) {
        case Qt.Key_Up:    case Qt.Key_W: previewUp = pressed;    return true
        case Qt.Key_Down:  case Qt.Key_S: previewDown = pressed;  return true
        case Qt.Key_Left:  case Qt.Key_A: previewLeft = pressed;  return true
        case Qt.Key_Right: case Qt.Key_D: previewRight = pressed; return true
        }
        return false
    }

    function clearPreviewKeys() {
        previewUp = false
        previewDown = false
        previewLeft = false
        previewRight = false
    }

    function advancePreview(frameTime) {
        const horizontal = (previewLeft ? 1 : 0) - (previewRight ? 1 : 0)
        const vertical = (previewUp ? 1 : 0) - (previewDown ? 1 : 0)
        if (horizontal === 0 && vertical === 0)
            return

        // Keep diagonal flight at the same speed as axial flight and cap a stalled
        // render frame so returning to the window cannot teleport the camera.
        const diagonalScale = horizontal !== 0 && vertical !== 0 ? Math.SQRT1_2 : 1
        const distance = previewSpeed * cell * Math.min(frameTime, 0.05) * diagonalScale
        viewX += horizontal * distance
        viewY += vertical * distance
    }

    onActiveChanged: {
        if (!active)
            clearPreviewKeys()
    }

    // Drives either free preview flight or the gameplay simulation once per rendered
    // frame. FrameAnimation follows requestAnimationFrame on web, so it keeps ticking
    // even when gameplay input is idle — unlike a QTimer.
    FrameAnimation {
        running: true
        onTriggered: {
            if (root.previewMode) {
                root.advancePreview(frameTime)
                const centerTileX = Math.floor((scene.width / 2 - root.viewX) / root.cell)
                const centerTileY = Math.floor((scene.height / 2 - root.viewY) / root.cell)
                board.generatePreviewAround(centerTileX, centerTileY)
            } else {
                board.update(frameTime * 1000)
                // Ease the camera toward its deadzone-constrained target. The factor is
                // derived from frameTime so the catch-up rate is frame-rate independent.
                const k = 1 - Math.exp(-frameTime / root.followTau)
                root.viewX += (root.cameraTarget(root.viewX, scene.width, board.playerX) - root.viewX) * k
                root.viewY += (root.cameraTarget(root.viewY, scene.height, board.playerY) - root.viewY) * k
                board.setVisibleArea(terrain.originX, terrain.originY,
                                     terrain.originX + terrain.cols - 1,
                                     terrain.originY + terrain.rows - 1)
            }
        }
    }

    // Full-window scene: owns keyboard focus and routes input. On web the canvas
    // needs active focus for key events, so we force it on load and on click.
    Item {
        id: scene
        anchors.fill: parent
        focus: true

        Component.onCompleted: forceActiveFocus()

        // Held-key movement: a press sets the direction, a release clears it. Ignore
        // auto-repeat so a held key is one sustained press, not a stream of events.
        Keys.onPressed: (event) => {
            if (event.isAutoRepeat) { event.accepted = true; return }
            if (root.previewMode && root.setPreviewKey(event.key, true)) {
                event.accepted = true
                return
            }
            if (root.previewMode) {
                if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal)
                    root.zoomPreview(1.25)
                else if (event.key === Qt.Key_Minus || event.key === Qt.Key_Underscore)
                    root.zoomPreview(0.8)
                else if (event.key === Qt.Key_R)
                    root.recenter()
                event.accepted = true
                return
            }
            switch (event.key) {
            case Qt.Key_Up:    case Qt.Key_W: board.setDirection(BoardModel.Up);    event.accepted = true; break
            case Qt.Key_Down:  case Qt.Key_S: board.setDirection(BoardModel.Down);  event.accepted = true; break
            case Qt.Key_Left:  case Qt.Key_A: board.setDirection(BoardModel.Left);  event.accepted = true; break
            case Qt.Key_Right: case Qt.Key_D: board.setDirection(BoardModel.Right); event.accepted = true; break
            case Qt.Key_Space:                board.placeBomb();                    event.accepted = true; break
            case Qt.Key_R:
                board.restart()
                root.recenter()
                event.accepted = true
                break
            }
        }

        Keys.onReleased: (event) => {
            if (event.isAutoRepeat) { event.accepted = true; return }
            if (root.previewMode && root.setPreviewKey(event.key, false)) {
                event.accepted = true
                return
            }
            if (root.previewMode) {
                event.accepted = true
                return
            }
            switch (event.key) {
            case Qt.Key_Up:    case Qt.Key_W: board.clearDirection(BoardModel.Up);    event.accepted = true; break
            case Qt.Key_Down:  case Qt.Key_S: board.clearDirection(BoardModel.Down);  event.accepted = true; break
            case Qt.Key_Left:  case Qt.Key_A: board.clearDirection(BoardModel.Left);  event.accepted = true; break
            case Qt.Key_Right: case Qt.Key_D: board.clearDirection(BoardModel.Right); event.accepted = true; break
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: scene.forceActiveFocus()
        }

        Item {
            id: boardView
            // No fixed size: the world is infinite. Children are positioned at their
            // GLOBAL tile coordinate (tile * cell); this Item just carries the camera
            // offset, smoothed in the per-frame tick.
            x: root.viewX
            y: root.viewY

            // Start centred on the spawn rather than easing in from (0, 0).
            Component.onCompleted: root.recenter()

            // Culled terrain: only the tiles overlapping the viewport (plus a one-tile
            // margin) are instantiated, each at its GLOBAL tile position. As the camera
            // scrolls, the window origin (floor of the camera offset) shifts, so a fixed
            // pool of rectangles recycles to new coordinates — the infinite world drawn
            // at bounded cost.
            Repeater {
                id: terrain
                readonly property int cols: Math.ceil(scene.width / root.cell) + 2
                readonly property int rows: Math.ceil(scene.height / root.cell) + 2
                readonly property int originX: Math.floor(-root.viewX / root.cell) - 1
                readonly property int originY: Math.floor(-root.viewY / root.cell) - 1

                // Preview uses one Canvas below. Keeping thousands of Rectangle
                // delegates alive was the dominant cost at maximum zoom-out.
                model: root.previewMode ? 0 : cols * rows

                TerrainTile {
                    required property int index

                    readonly property int gx: terrain.originX + (index % terrain.cols)
                    readonly property int gy: terrain.originY + Math.floor(index / terrain.cols)
                    // Re-read on scroll (gx/gy change) and on board changes (revision),
                    // so streamed-in tiles and bombed-open bricks both show.
                    tile: {
                        board.revision
                        return board.tileAt(gx, gy)
                    }
                    pal: root.tierColors[
                        Math.min(board.tierAt(gx, gy), root.tierColors.length - 1)]

                    cell: root.cell
                    x: gx * root.cell
                    y: gy * root.cell
                }
            }

            // Preview terrain is rendered as one scene-graph item. It repaints only
            // when the integer tile origin, zoom, or discovered chunk set changes;
            // sub-tile camera motion moves this cached image with boardView.
            Canvas {
                id: previewTerrain

                visible: root.previewMode
                antialiasing: false

                readonly property int originX: terrain.originX
                readonly property int originY: terrain.originY
                readonly property int cols: terrain.cols
                readonly property int rows: terrain.rows
                readonly property real tileSize: root.cell
                readonly property int renderedRevision: board.revision

                x: originX * tileSize
                y: originY * tileSize
                width: cols * tileSize
                height: rows * tileSize

                onOriginXChanged: requestPaint()
                onOriginYChanged: requestPaint()
                onColsChanged: requestPaint()
                onRowsChanged: requestPaint()
                onTileSizeChanged: requestPaint()
                onRenderedRevisionChanged: requestPaint()

                onPaint: {
                    const context = getContext("2d")
                    context.clearRect(0, 0, width, height)
                    context.fillStyle = root.unknownTerrain
                    context.fillRect(0, 0, width, height)

                    // Chunk-scale grid is visible only where no generated tile later
                    // paints over it, making unexplored space unambiguous from Void.
                    context.strokeStyle = root.unknownGrid
                    context.lineWidth = 1
                    context.beginPath()
                    for (let col = 0; col <= cols; ++col) {
                        const gx = originX + col
                        if ((gx % root.previewChunkTiles + root.previewChunkTiles)
                                % root.previewChunkTiles === 0) {
                            const x = col * tileSize + 0.5
                            context.moveTo(x, 0)
                            context.lineTo(x, height)
                        }
                    }
                    for (let row = 0; row <= rows; ++row) {
                        const gy = originY + row
                        if ((gy % root.previewChunkTiles + root.previewChunkTiles)
                                % root.previewChunkTiles === 0) {
                            const y = row * tileSize + 0.5
                            context.moveTo(0, y)
                            context.lineTo(width, y)
                        }
                    }
                    context.stroke()

                    const drawBorders = tileSize >= 14
                    const firstChunkX = Math.floor(originX / root.previewChunkTiles)
                    const firstChunkY = Math.floor(originY / root.previewChunkTiles)
                    const lastChunkX = Math.floor((originX + cols - 1)
                                                  / root.previewChunkTiles)
                    const lastChunkY = Math.floor((originY + rows - 1)
                                                  / root.previewChunkTiles)
                    for (let chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY) {
                        for (let chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX) {
                            if (!board.previewChunkGenerated(chunkX, chunkY))
                                continue

                            const pal = root.tierColors[
                                Math.min(board.tierAt(chunkX * root.previewChunkTiles,
                                                      chunkY * root.previewChunkTiles),
                                         root.tierColors.length - 1)]
                            const firstX = Math.max(originX, chunkX * root.previewChunkTiles)
                            const firstY = Math.max(originY, chunkY * root.previewChunkTiles)
                            const lastX = Math.min(originX + cols,
                                                   (chunkX + 1) * root.previewChunkTiles)
                            const lastY = Math.min(originY + rows,
                                                   (chunkY + 1) * root.previewChunkTiles)
                            for (let gy = firstY; gy < lastY; ++gy) {
                                for (let gx = firstX; gx < lastX; ++gx) {
                                    const tile = board.previewTileAt(gx, gy)
                                    context.fillStyle = tile === BoardModel.Wall ? pal.rock
                                        : tile === BoardModel.Brick ? pal.brick
                                        : tile === BoardModel.Void ? pal.abyss
                                        : pal.floor
                                    const x = (gx - originX) * tileSize
                                    const y = (gy - originY) * tileSize
                                    context.fillRect(x, y, Math.ceil(tileSize), Math.ceil(tileSize))
                                    if (drawBorders && tile !== BoardModel.Void) {
                                        context.strokeStyle = "#2a2a2a"
                                        context.lineWidth = 1
                                        context.strokeRect(x + 0.5, y + 0.5,
                                                           tileSize - 1, tileSize - 1)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Power-ups
            Repeater {
                model: board.powerUpCount

                Rectangle {
                    required property int index

                    readonly property int kind: board.powerUpType(index)

                    width: root.cell * 0.42
                    height: root.cell * 0.42
                    radius: 3
                    rotation: 45
                    color: kind === BoardModel.BombLimitPowerUp ? "#2fb8ac"
                         : kind === BoardModel.BombRangePowerUp ? "#4f8cff"
                         : "#d85ce6"
                    border.color: "#111111"
                    border.width: 2
                    x: board.powerUpX(index) * root.cell + (root.cell - width) / 2
                    y: board.powerUpY(index) * root.cell + (root.cell - height) / 2
                }
            }

            // Perk crystals: the level-up reward cluster on the floor. Walk onto one to
            // claim its perk; the rest vanish. Bigger, gold-rimmed and pulsing so they
            // read as "special" loot next to the small brick power-ups, with the perk
            // name floating above. Re-read on revision (a claim/level-up swaps the set).
            Repeater {
                model: board.perkCrystalCount

                Item {
                    required property int index
                    readonly property string fill: { board.revision; return board.perkCrystalColor(index) }

                    width: root.cell
                    height: root.cell
                    x: { board.revision; return board.perkCrystalX(index) * root.cell }
                    y: { board.revision; return board.perkCrystalY(index) * root.cell }

                    Rectangle {
                        anchors.centerIn: parent
                        width: root.cell * 0.5
                        height: width
                        radius: 4
                        rotation: 45
                        color: parent.fill
                        border.color: "#fff1c0"
                        border.width: 3

                        SequentialAnimation on scale {
                            loops: Animation.Infinite
                            running: true
                            NumberAnimation { from: 0.88; to: 1.12; duration: 650; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 1.12; to: 0.88; duration: 650; easing.type: Easing.InOutSine }
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 1
                        text: { board.revision; return board.perkCrystalName(index) }
                        color: "#fff1c0"
                        font.pixelSize: 11
                        font.bold: true
                        style: Text.Outline
                        styleColor: "#000000"
                    }
                }
            }

            // Bombs
            Repeater {
                model: board.bombCount

                BombSprite {
                    required property int index

                    cell: root.cell
                    // bombFuse is a plain call, so re-read on revision (bumped every
                    // tick) to track the fuse burning down toward detonation.
                    fuse: { board.revision; return board.bombFuse(index) }
                    x: board.bombX(index) * root.cell
                    y: board.bombY(index) * root.cell
                }
            }

            // Explosion flames
            Repeater {
                model: board.explosionCount

                FlameSprite {
                    required property int index

                    cell: root.cell
                    // explosionLife is a plain call, so re-read on revision (bumped
                    // every tick) to follow the flame burning out.
                    life: { board.revision; return board.explosionLife(index) }
                    x: board.explosionX(index) * root.cell
                    y: board.explosionY(index) * root.cell
                }
            }

            // Enemies — fractional tile units from the core, like the player.
            Repeater {
                model: board.enemyCount

                EnemySprite {
                    required property int index

                    // Appearance comes entirely from the model — no per-archetype
                    // logic here. Re-read on revision (bumped every tick): a kill
                    // shifts indices, so the enemy at this slot can change.
                    cell: root.cell
                    fill: { board.revision; return board.enemyColor(index) }
                    kind: { board.revision; return board.enemyKind(index) }
                    // enemyX/Y are plain calls, not notifying properties, so depend
                    // on revision (bumped every tick) to re-read the moving position.
                    cellX: { board.revision; return board.enemyX(index) }
                    cellY: { board.revision; return board.enemyY(index) }
                    x: cellX * root.cell
                    y: cellY * root.cell
                }
            }

            // Player — position comes from the core in fractional tile units, so the
            // continuous movement renders directly (no Behavior easing needed). The
            // sprite derives its heading and walk cycle from those moving coordinates.
            PlayerSprite {
                cell: root.cell
                cellX: board.playerX
                cellY: board.playerY
                x: board.playerX * root.cell
                y: board.playerY * root.cell
            }
        }

        // Run HUD: current level and an XP bar toward the next. Top-left, never grabs
        // input so focus-on-click still works.
        Column {
            visible: !root.previewMode
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 4

            Text {
                text: "Lv " + board.level
                color: "#ffe08a"
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle {
                width: 130
                height: 9
                radius: 4
                color: "#2a2a2a"
                border.color: "#000000"
                border.width: 1

                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    width: parent.width
                         * Math.min(1, board.xp / Math.max(1, board.xpToNextLevel))
                    color: "#ffd24a"
                }
            }
        }

        Rectangle {
            visible: root.previewMode
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            width: previewLabel.implicitWidth + 20
            height: previewLabel.implicitHeight + 12
            radius: 5
            color: Qt.rgba(0, 0, 0, 0.72)
            border.color: "#69d2ff"

            Text {
                id: previewLabel
                anchors.centerIn: parent
                text: "PREVIEW · WASD / arrows · + / − zoom · R resets view · "
                    + Math.round(root.cell / root.defaultCell * 100) + "% zoom · "
                    + board.previewChunkCount + " chunks · grid = unexplored"
                color: "#9be3ff"
                font.pixelSize: 15
                font.bold: true
            }
        }

        // End-of-run overlay: dims the world and offers a restart on death. The
        // streamed world is endless, so the only end state is Lost.
        Rectangle {
            anchors.fill: parent
            visible: board.state === BoardModel.Lost
            color: Qt.rgba(0, 0, 0, 0.6)

            MouseArea {
                anchors.fill: parent
                onClicked: { board.restart(); root.recenter(); scene.forceActiveFocus() }
            }

            Column {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Game Over"
                    color: "#ff6b6b"
                    font.pixelSize: 44
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Press R or click to restart"
                    color: "#dddddd"
                    font.pixelSize: 18
                }
            }
        }

        // Build stamp: which commit/date is actually running. Lives in the
        // corner so the deployed web build (and every PR preview) is verifiable
        // at a glance. Doesn't grab input, so focus-on-click still works.
        Text {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 6

            text: board.version
            color: "#cccccc"
            opacity: 0.55
            font.pixelSize: 12
            font.family: "monospace"
        }
    }
}
