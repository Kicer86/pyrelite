
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

    Item {
        id: boardView
        width: board.columns * root.cell
        height: board.rows * root.cell
        anchors.centerIn: parent

        focus: true
        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Up:    case Qt.Key_W: board.moveUp();    event.accepted = true; break
            case Qt.Key_Down:  case Qt.Key_S: board.moveDown();  event.accepted = true; break
            case Qt.Key_Left:  case Qt.Key_A: board.moveLeft();  event.accepted = true; break
            case Qt.Key_Right: case Qt.Key_D: board.moveRight(); event.accepted = true; break
            }
        }

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
                    readonly property int tile: board.tileAt(cx, cy)

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
