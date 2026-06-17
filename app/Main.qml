
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
        onTriggered: board.update(Math.round(frameTime * 1000))
    }

    // Full-window scene: owns keyboard focus and routes input. On web the canvas
    // needs active focus for key events, so we force it on load and on click.
    Item {
        id: scene
        anchors.fill: parent
        focus: true

        Component.onCompleted: forceActiveFocus()

        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Up:    case Qt.Key_W: board.moveUp();    event.accepted = true; break
            case Qt.Key_Down:  case Qt.Key_S: board.moveDown();  event.accepted = true; break
            case Qt.Key_Left:  case Qt.Key_A: board.moveLeft();  event.accepted = true; break
            case Qt.Key_Right: case Qt.Key_D: board.moveRight(); event.accepted = true; break
            case Qt.Key_Space:                board.placeBomb(); event.accepted = true; break
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

            // Player
            Rectangle {
                width: root.cell * 0.7
                height: root.cell * 0.7
                radius: width / 2
                color: "#e0d040"
                border.color: "#3a3210"
                border.width: 2
                x: board.playerX * root.cell + (root.cell - width) / 2
                y: board.playerY * root.cell + (root.cell - height) / 2

                Behavior on x { NumberAnimation { duration: 80 } }
                Behavior on y { NumberAnimation { duration: 80 } }
            }
        }
    }
}
