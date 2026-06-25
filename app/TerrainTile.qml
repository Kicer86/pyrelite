
import QtQuick
import PyreliteApp

// One terrain cell, drawn from the tier palette (floor / rock / brick / abyss).
// Kept deliberately light — a handful of child rectangles, no animation — because
// the view instantiates a culled pool of these (one per visible tile) and recycles
// them as the camera scrolls. Each tile type renders its own look; the model only
// reports which type and which palette band applies.
Item {
    id: tileItem

    property real cell: 48
    property int tile: BoardModel.Empty
    // Tier palette band: { floor, rock, brick, abyss }.
    property var pal

    width: cell
    height: cell

    readonly property bool isFloor: tile === BoardModel.Empty
    readonly property bool isWall: tile === BoardModel.Wall
    readonly property bool isBrick: tile === BoardModel.Brick
    readonly property bool isVoid: tile === BoardModel.Void

    // Floor: a faintly top-lit channel with a thin grid seam, so open space still
    // reads as a tiled surface without distracting from the actors on top of it.
    Rectangle {
        anchors.fill: parent
        visible: tileItem.isFloor
        border.color: "#2a2a2a"
        border.width: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.lighter(tileItem.pal.floor, 1.12) }
            GradientStop { position: 1.0; color: Qt.darker(tileItem.pal.floor, 1.12) }
        }
        // Inset darker patch for a touch of ground texture.
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.5
            height: parent.height * 0.5
            radius: 3
            color: Qt.darker(tileItem.pal.floor, 1.1)
            opacity: 0.25
        }
    }

    // Void: a deep abyss, darkening toward the bottom with an inner top shadow so it
    // reads as a pit dropping away rather than a flat panel. No grid seam.
    Rectangle {
        anchors.fill: parent
        visible: tileItem.isVoid
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.darker(tileItem.pal.abyss, 1.6) }
            GradientStop { position: 0.55; color: tileItem.pal.abyss }
            GradientStop { position: 1.0; color: Qt.darker(tileItem.pal.abyss, 2.2) }
        }
    }

    // Rock wall: a bevelled stone block. A dark groove frames a top-lit inner face,
    // with a light top-left edge and a shadowed bottom-right edge for relief.
    Item {
        anchors.fill: parent
        visible: tileItem.isWall

        Rectangle {
            anchors.fill: parent
            color: Qt.darker(tileItem.pal.rock, 1.9) // groove / outline
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: Math.max(1, tileItem.cell * 0.05)
            radius: tileItem.cell * 0.08
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(tileItem.pal.rock, 1.3) }
                GradientStop { position: 0.5; color: tileItem.pal.rock }
                GradientStop { position: 1.0; color: Qt.darker(tileItem.pal.rock, 1.4) }
            }
        }
        // Top-left highlight edge.
        Rectangle {
            x: tileItem.cell * 0.05; y: tileItem.cell * 0.05
            width: parent.width - tileItem.cell * 0.1
            height: Math.max(1, tileItem.cell * 0.06)
            color: "#ffffff"; opacity: 0.16
            radius: height / 2
        }
    }

    // Brick: a top-lit face cut by mortar — two horizontal courses and staggered
    // vertical joints — so destructible walls read distinctly from solid rock.
    Item {
        anchors.fill: parent
        visible: tileItem.isBrick

        Rectangle {
            anchors.fill: parent
            radius: tileItem.cell * 0.04
            border.color: Qt.darker(tileItem.pal.brick, 1.8)
            border.width: 1
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(tileItem.pal.brick, 1.25) }
                GradientStop { position: 1.0; color: Qt.darker(tileItem.pal.brick, 1.2) }
            }
        }

        readonly property color mortar: Qt.darker(tileItem.pal.brick, 1.9)
        readonly property real joint: Math.max(1, tileItem.cell * 0.045)

        // Two horizontal mortar courses, splitting the face into three brick rows.
        Repeater {
            model: 2
            Rectangle {
                required property int index
                width: parent.width
                height: parent.joint
                color: parent.mortar
                y: parent.height * (index === 0 ? 0.34 : 0.67) - height / 2
            }
        }
        // Vertical joints, staggered per row for a running-bond look.
        Repeater {
            model: 3
            Rectangle {
                required property int index
                width: parent.joint
                height: parent.height / 3 - parent.joint
                color: parent.mortar
                x: parent.width * (index % 2 === 0 ? 0.5 : 0.25) - width / 2
                y: parent.height / 3 * index + parent.joint / 2
            }
        }
        // Top highlight edge across the whole brick.
        Rectangle {
            x: tileItem.cell * 0.06; y: tileItem.cell * 0.05
            width: parent.width - tileItem.cell * 0.12
            height: Math.max(1, tileItem.cell * 0.05)
            color: "#ffffff"; opacity: 0.14
            radius: height / 2
        }
    }
}
