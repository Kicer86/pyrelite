
import QtQuick

// Top-down bomber avatar drawn procedurally (no art assets yet): a rounded body,
// a helmet that turns to the heading, eyes that glance the same way, and little
// feet that step through a walk cycle while bobbing. The heading is derived purely
// from movement deltas, so the core/model stay untouched — this is view-only feel.
Item {
    id: sprite

    // Fractional tile position from the model and the pixel size of one tile. The
    // parent positions this Item at the tile's pixel origin; the figure sits centred.
    property real cellX: 0
    property real cellY: 0
    property real cell: 48

    // Body palette. Defaults to the warm bomber yellow the placeholder used.
    property color bodyColor: "#e6c93a"
    property color trimColor: "#b5532a"

    width: cell
    height: cell

    // Heading: 0 Down, 1 Up, 2 Left, 3 Right. Updated from the sign of the larger
    // movement axis; held when idle so the figure keeps facing where it last walked.
    property int facing: 0
    property bool moving: false

    property real _prevX: cellX
    property real _prevY: cellY

    function _updateHeading() {
        const dx = cellX - _prevX
        const dy = cellY - _prevY
        _prevX = cellX
        _prevY = cellY
        const eps = 0.0005
        if (Math.abs(dx) < eps && Math.abs(dy) < eps)
            return
        if (Math.abs(dx) > Math.abs(dy))
            facing = dx > 0 ? 3 : 2
        else
            facing = dy > 0 ? 0 : 1
        moving = true
        idleTimer.restart()
    }

    onCellXChanged: _updateHeading()
    onCellYChanged: _updateHeading()

    // The figure stops "walking" shortly after position updates cease.
    Timer {
        id: idleTimer
        interval: 90
        onTriggered: sprite.moving = false
    }

    // Walk phase in [-1, 1], driving foot swing and the body bob. Idle parks it at 0.
    property real _gait: 0
    SequentialAnimation on _gait {
        running: sprite.moving
        loops: Animation.Infinite
        NumberAnimation { from: -1; to: 1; duration: 170; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: -1; duration: 170; easing.type: Easing.InOutSine }
    }
    onMovingChanged: if (!moving) _gait = 0

    // Heading offsets: how far the helmet/eyes lean toward the facing direction.
    readonly property real _leanX: facing === 2 ? -1 : facing === 3 ? 1 : 0
    readonly property real _leanY: facing === 1 ? -1 : facing === 0 ? 1 : 0

    // Centre of the tile, where the figure is built around.
    readonly property real cx: cell / 2
    readonly property real cy: cell / 2
    readonly property real r: cell * 0.30   // body radius

    // Soft contact shadow; squashes a touch as the body bobs up.
    Rectangle {
        width: sprite.r * 2.05
        height: sprite.r * 0.85
        radius: height / 2
        color: "#000000"
        opacity: 0.28
        x: sprite.cx - width / 2
        y: sprite.cy + sprite.r * 0.55
    }

    // Everything that bobs lives in this group so feet stay planted.
    Item {
        anchors.fill: parent
        y: sprite.moving ? -Math.abs(sprite._gait) * sprite.r * 0.12 : 0
        Behavior on y { NumberAnimation { duration: 60 } }

        // Back foot / front foot, swung in counter-phase along the heading axis.
        Repeater {
            model: 2
            Rectangle {
                required property int index
                readonly property real swing: (index === 0 ? sprite._gait : -sprite._gait)
                width: sprite.r * 0.5
                height: sprite.r * 0.62
                radius: width / 2
                color: Qt.darker(sprite.trimColor, 1.4)
                x: sprite.cx + (index === 0 ? -sprite.r * 0.45 : sprite.r * 0.45) - width / 2
                   + swing * sprite._leanX * sprite.r * 0.35
                y: sprite.cy + sprite.r * 0.5 - height / 2
                   + swing * sprite._leanY * sprite.r * 0.35
            }
        }

        // Body: rounded torso with top-lit vertical shading and a dark outline.
        Rectangle {
            width: sprite.r * 2
            height: sprite.r * 2.1
            radius: sprite.r * 0.8
            x: sprite.cx - width / 2
            y: sprite.cy - height / 2
            border.color: Qt.darker(sprite.bodyColor, 2.4)
            border.width: Math.max(1, sprite.cell * 0.035)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(sprite.bodyColor, 1.35) }
                GradientStop { position: 0.45; color: sprite.bodyColor }
                GradientStop { position: 1.0; color: Qt.darker(sprite.bodyColor, 1.45) }
            }

            // Belly highlight to give the torso a little volume.
            Rectangle {
                width: parent.width * 0.42
                height: parent.height * 0.5
                radius: width / 2
                x: parent.width * 0.16
                y: parent.height * 0.14
                color: "#ffffff"
                opacity: 0.16
            }
        }

        // Helmet cap: leans toward the heading so the figure reads as turning.
        Rectangle {
            width: sprite.r * 1.7
            height: sprite.r * 1.1
            radius: width / 2
            x: sprite.cx - width / 2 + sprite._leanX * sprite.r * 0.28
            y: sprite.cy - sprite.r * 0.95 + sprite._leanY * sprite.r * 0.22
            border.color: Qt.darker(sprite.trimColor, 1.8)
            border.width: Math.max(1, sprite.cell * 0.03)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(sprite.trimColor, 1.3) }
                GradientStop { position: 1.0; color: Qt.darker(sprite.trimColor, 1.2) }
            }
            Behavior on x { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
            Behavior on y { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        }

        // Eyes: a pair that glances along the heading. Hidden when facing away (Up).
        Repeater {
            model: 2
            Rectangle {
                required property int index
                readonly property real side: index === 0 ? -1 : 1
                visible: sprite.facing !== 1
                width: sprite.r * 0.42
                height: width
                radius: width / 2
                color: "#fdfdfd"
                x: sprite.cx + side * sprite.r * 0.42 - width / 2
                   + sprite._leanX * sprite.r * 0.18
                y: sprite.cy - sprite.r * 0.12 - height / 2
                   + sprite._leanY * sprite.r * 0.18
                Behavior on x { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }

                // Pupil, nudged a little further toward the heading.
                Rectangle {
                    width: parent.width * 0.55
                    height: width
                    radius: width / 2
                    color: "#1a1410"
                    x: parent.width / 2 - width / 2 + sprite._leanX * parent.width * 0.28
                    y: parent.height / 2 - height / 2 + sprite._leanY * parent.height * 0.28
                }
            }
        }
    }
}
