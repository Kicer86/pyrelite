
import QtQuick
import QtQuick.Window
import PyreliteApp

Window {
    id: root
    visible: true
    title: "Pyrelite"
    color: "#1b1b1b"

    BoardModel { id: board }

    // On the web, fill the browser viewport; on desktop use a comfortable
    // resizable window. Either way the board scales to fit (below).
    readonly property bool onWeb: Qt.platform.os === "wasm"
    width: onWeb ? Screen.width : 960
    height: onWeb ? Screen.height : 820

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
