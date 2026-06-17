
import QtQuick
import QtQuick.Window
import PyreliteApp

Window {
    id: root

    // On the web each HTML container is a QScreen; a fullscreen window uses the
    // entire screen area, so this fills the (full-viewport) canvas. On desktop
    // use a normal resizable window. Either way the board scales to fit (below).
    visibility: Qt.platform.os === "wasm" ? Window.FullScreen : Window.Windowed
    width: 960
    height: 820

    title: "Pyrelite"
    color: "#1b1b1b"

    BoardModel { id: board }

    // Square cells sized to fill the available area, preserving aspect ratio.
    readonly property real cell: Math.floor(Math.min(width / board.columns,
                                                      height / board.rows))

    Item {
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
    }
}
