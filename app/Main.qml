
import QtQuick
import QtQuick.Window
import PyreliteApp

Window {
    id: root
    visible: true
    width: board.columns * cell
    height: board.rows * cell
    title: "Pyrelite"
    color: "#1b1b1b"

    readonly property int cell: 40

    BoardModel { id: board }

    Grid {
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
