
import QtQuick
import QtQuick.Window
import PyreliteApp

Window {
    id: root

    visibility: Qt.platform.os === "wasm" ? Window.FullScreen : Window.Windowed
    width: 960
    height: 820

    title: "Pyrelite"
    color: "#1b1b1b"

    BoardModel { id: board }

    // Fixed tile size in pixels so an arena can be larger than the window; the
    // camera (boardView below) scrolls to keep the player in view, rather than
    // shrinking the whole board to fit one screen.
    readonly property real cell: 48

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

    // Drives bomb fuses + explosions in the core, once per rendered frame.
    // FrameAnimation is tied to the render loop (requestAnimationFrame on web),
    // so it keeps ticking even when idle — unlike a QTimer.
    FrameAnimation {
        running: true
        onTriggered: {
            board.setVisibleArea(terrain.originX, terrain.originY,
                                 terrain.originX + terrain.cols - 1,
                                 terrain.originY + terrain.rows - 1)
            board.update(frameTime * 1000)
            // Ease the camera toward its deadzone-constrained target. The factor is
            // derived from frameTime so the catch-up rate is frame-rate independent.
            const k = 1 - Math.exp(-frameTime / root.followTau)
            root.viewX += (root.cameraTarget(root.viewX, scene.width, board.playerX) - root.viewX) * k
            root.viewY += (root.cameraTarget(root.viewY, scene.height, board.playerY) - root.viewY) * k
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
            switch (event.key) {
            case Qt.Key_Up:    case Qt.Key_W: board.setDirection(BoardModel.Up);    event.accepted = true; break
            case Qt.Key_Down:  case Qt.Key_S: board.setDirection(BoardModel.Down);  event.accepted = true; break
            case Qt.Key_Left:  case Qt.Key_A: board.setDirection(BoardModel.Left);  event.accepted = true; break
            case Qt.Key_Right: case Qt.Key_D: board.setDirection(BoardModel.Right); event.accepted = true; break
            case Qt.Key_Space:                board.placeBomb();                    event.accepted = true; break
            case Qt.Key_R:                    board.restart(); root.recenter();     event.accepted = true; break
            }
        }

        Keys.onReleased: (event) => {
            if (event.isAutoRepeat) { event.accepted = true; return }
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

                model: cols * rows

                Rectangle {
                    required property int index

                    readonly property int gx: terrain.originX + (index % terrain.cols)
                    readonly property int gy: terrain.originY + Math.floor(index / terrain.cols)
                    // Re-read on scroll (gx/gy change) and on board changes (revision),
                    // so streamed-in tiles and bombed-open bricks both show.
                    readonly property int tile: {
                        board.revision
                        return board.tileAt(gx, gy)
                    }
                    readonly property var pal: root.tierColors[
                        Math.min(board.tierAt(gx, gy), root.tierColors.length - 1)]

                    width: root.cell
                    height: root.cell
                    x: gx * root.cell
                    y: gy * root.cell
                    color: tile === BoardModel.Wall ? pal.rock
                         : tile === BoardModel.Brick ? pal.brick
                         : tile === BoardModel.Void ? pal.abyss
                         : pal.floor
                    // No grid line over the void, so it reads as continuous open space.
                    border.color: tile === BoardModel.Void ? pal.abyss : "#2a2a2a"
                    border.width: 1
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

                Rectangle {
                    required property int index

                    width: root.cell * 0.6
                    height: root.cell * 0.6
                    radius: width / 2
                    color: "#222222"
                    border.color: "#000000"
                    border.width: 2
                    x: board.bombX(index) * root.cell + (root.cell - width) / 2
                    y: board.bombY(index) * root.cell + (root.cell - height) / 2
                }
            }

            // Explosion flames
            Repeater {
                model: board.explosionCount

                Rectangle {
                    required property int index

                    width: root.cell * 0.92
                    height: root.cell * 0.92
                    radius: 4
                    color: "#ff8c1a"
                    x: board.explosionX(index) * root.cell + (root.cell - width) / 2
                    y: board.explosionY(index) * root.cell + (root.cell - height) / 2
                }
            }

            // Enemies — fractional tile units from the core, like the player.
            Repeater {
                model: board.enemyCount

                Rectangle {
                    required property int index

                    // Appearance comes entirely from the model — no per-archetype
                    // logic here. Re-read on revision (bumped every tick): a kill
                    // shifts indices, so the enemy at this slot can change.
                    readonly property color fill: { board.revision; return board.enemyColor(index) }

                    width: root.cell * 0.7
                    height: root.cell * 0.7
                    radius: width / 2
                    color: fill
                    border.color: Qt.darker(fill, 2.2)
                    border.width: 2
                    // enemyX/Y are plain calls, not notifying properties, so depend
                    // on revision (bumped every tick) to re-read the moving position.
                    x: { board.revision; return board.enemyX(index) * root.cell + (root.cell - width) / 2 }
                    y: { board.revision; return board.enemyY(index) * root.cell + (root.cell - height) / 2 }
                }
            }

            // Player — position comes from the core in fractional tile units, so the
            // continuous movement renders directly (no Behavior easing needed).
            Rectangle {
                width: root.cell * 0.7
                height: root.cell * 0.7
                radius: width / 2
                color: "#e0d040"
                border.color: "#3a3210"
                border.width: 2
                x: board.playerX * root.cell + (root.cell - width) / 2
                y: board.playerY * root.cell + (root.cell - height) / 2
            }
        }

        // Run HUD: current level and an XP bar toward the next. Top-left, never grabs
        // input so focus-on-click still works.
        Column {
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
