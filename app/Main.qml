
import QtQuick
import QtQuick.Window

Window {
    id: root
    visible: true
    width: cols * cell
    height: rows * cell
    title: "Pyrelite"
    color: "#1b1b1b"

    // Placeholder board (not yet driven by the C++ core — that comes in M1).
    readonly property int cols: 13
    readonly property int rows: 11
    readonly property int cell: 40

    Grid {
        columns: root.cols
        rows: root.rows

        Repeater {
            model: root.cols * root.rows

            Rectangle {
                required property int index

                readonly property int cx: index % root.cols
                readonly property int cy: Math.floor(index / root.cols)
                readonly property bool wall: cx === 0 || cy === 0
                                             || cx === root.cols - 1 || cy === root.rows - 1
                                             || (cx % 2 === 0 && cy % 2 === 0)

                width: root.cell
                height: root.cell
                color: wall ? "#555a5e" : "#3a7d3a"
                border.color: "#2a2a2a"
                border.width: 1
            }
        }
    }
}
