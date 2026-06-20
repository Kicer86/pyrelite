
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

    readonly property real cell: Math.floor(Math.min(width / board.columns,
                                                      height / board.rows))

    // Drives bomb fuses + explosions in the core, once per rendered frame.
    // FrameAnimation is tied to the render loop (requestAnimationFrame on web),
    // so it keeps ticking even when idle — unlike a QTimer.
    FrameAnimation {
        running: true
        onTriggered: board.update(frameTime * 1000)
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
            case Qt.Key_R:                    board.restart();                      event.accepted = true; break
            // Pick a perk during a level-up. Ignored (a no-op in the core) otherwise.
            case Qt.Key_1:                    board.choosePerk(0);                  event.accepted = true; break
            case Qt.Key_2:                    board.choosePerk(1);                  event.accepted = true; break
            case Qt.Key_3:                    board.choosePerk(2);                  event.accepted = true; break
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
            width: board.columns * root.cell
            height: board.rows * root.cell
            anchors.centerIn: parent

            Grid {
                anchors.fill: parent
                columns: board.columns
                rows: board.rows

                Repeater {
                    model: board.columns * board.rows

                    Rectangle {
                        required property int index

                        readonly property int cx: index % board.columns
                        readonly property int cy: Math.floor(index / board.columns)
                        // Re-read the tile whenever the board changes (revision),
                        // so destroyed bricks turn to floor.
                        readonly property int tile: {
                            board.revision
                            return board.tileAt(cx, cy)
                        }

                        width: root.cell
                        height: root.cell
                        color: tile === BoardModel.Wall ? "#555a5e"
                             : tile === BoardModel.Brick ? "#9c6b3c"
                             : "#3a7d3a"
                        border.color: "#2a2a2a"
                        border.width: 1
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

        // Level-up overlay: freezes the arena and offers a perk to pick — click a card
        // or press 1-3. Mirrors the end-of-run overlay's dim-and-choose pattern.
        Rectangle {
            anchors.fill: parent
            visible: board.state === BoardModel.LevelUp
            color: Qt.rgba(0, 0, 0, 0.72)

            // Swallow clicks so they don't fall through to the board's focus handler.
            MouseArea { anchors.fill: parent }

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Level " + board.level + " — choose a perk"
                    color: "#ffe08a"
                    font.pixelSize: 34
                    font.bold: true
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16

                    Repeater {
                        model: board.perkChoiceCount

                        Rectangle {
                            required property int index

                            width: 200
                            height: 150
                            radius: 10
                            color: cardMouse.containsMouse ? "#3a3320" : "#262626"
                            border.color: "#ffd24a"
                            border.width: 2

                            Column {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 10

                                // Re-read on revision: choosePerk may open the next
                                // banked offer without changing the card count, so the
                                // Repeater would not rebuild on its own.
                                Text {
                                    width: parent.width
                                    text: { board.revision
                                            return (index + 1) + ". " + board.perkName(index) }
                                    color: "#ffe08a"
                                    font.pixelSize: 20
                                    font.bold: true
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    width: parent.width
                                    text: { board.revision; return board.perkDescription(index) }
                                    color: "#dddddd"
                                    font.pixelSize: 15
                                    wrapMode: Text.WordWrap
                                }
                            }

                            MouseArea {
                                id: cardMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: { board.choosePerk(index); scene.forceActiveFocus() }
                            }
                        }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Click a card or press 1-3"
                    color: "#aaaaaa"
                    font.pixelSize: 15
                }
            }
        }

        // End-of-run overlay: dims the arena and offers a restart on win or loss.
        Rectangle {
            anchors.fill: parent
            visible: board.state === BoardModel.Won || board.state === BoardModel.Lost
            color: Qt.rgba(0, 0, 0, 0.6)

            MouseArea {
                anchors.fill: parent
                onClicked: { board.restart(); scene.forceActiveFocus() }
            }

            Column {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: board.state === BoardModel.Won ? "Arena cleared!" : "Game Over"
                    color: board.state === BoardModel.Won ? "#7ee07e" : "#ff6b6b"
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
