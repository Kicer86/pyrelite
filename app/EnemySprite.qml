
import QtQuick

// Procedural enemy creature (no art assets yet). A shared blob body — shaded,
// with eyes that glance toward its heading and a breathing idle wobble — plus a
// per-archetype silhouette layered on top: the model hands us a `kind` and a body
// `fill`, and nothing here knows the core archetypes beyond those two strings.
Item {
    id: sprite

    property real cellX: 0
    property real cellY: 0
    property real cell: 48

    // Presentation seam from BoardModel: body tint and which silhouette to draw.
    property color fill: "#d23b3b"
    property string kind: "wanderer"

    width: cell
    height: cell

    // Heading from movement deltas, like the player — drives the eye glance only.
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

    Timer { id: idleTimer; interval: 110; onTriggered: sprite.moving = false }

    readonly property real _glX: facing === 2 ? -1 : facing === 3 ? 1 : 0
    readonly property real _glY: facing === 1 ? -1 : facing === 0 ? 1 : 0

    readonly property real cx: cell / 2
    readonly property real cy: cell / 2
    readonly property real r: cell * 0.30

    readonly property bool isGhost: kind === "ghost"
    readonly property bool isBouncer: kind === "bouncer"

    // Breathing wobble: a slow squash-stretch, quicker while on the move.
    property real _breath: 0
    SequentialAnimation on _breath {
        running: true
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: sprite.moving ? 200 : 420; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: sprite.moving ? 200 : 420; easing.type: Easing.InOutSine }
    }

    // Bouncer hops continuously; others stay grounded.
    property real _hop: 0
    SequentialAnimation on _hop {
        running: sprite.isBouncer
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 300; easing.type: Easing.OutQuad }
        NumberAnimation { from: 1; to: 0; duration: 300; easing.type: Easing.InQuad }
    }

    // Ghost drifts up and down in place.
    property real _float: 0
    SequentialAnimation on _float {
        running: sprite.isGhost
        loops: Animation.Infinite
        NumberAnimation { from: -1; to: 1; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: -1; duration: 900; easing.type: Easing.InOutSine }
    }

    opacity: isGhost ? 0.72 : 1.0

    // Contact shadow; shrinks as a bouncer leaves the ground, gone for the ghost.
    Rectangle {
        visible: !sprite.isGhost
        width: sprite.r * 2 * (1 - sprite._hop * 0.4)
        height: sprite.r * 0.7
        radius: height / 2
        color: "#000000"
        opacity: 0.26 * (1 - sprite._hop * 0.5)
        x: sprite.cx - width / 2
        y: sprite.cy + sprite.r * 0.6
    }

    // The creature body group: carries the squash, hop and ghost float.
    Item {
        anchors.fill: parent
        y: -sprite._hop * sprite.r * 0.7 + (sprite.isGhost ? sprite._float * sprite.r * 0.18 : 0)

        // Chaser horns, behind the body so they read as growing out of the head.
        Repeater {
            model: sprite.kind === "chaser" ? 2 : 0
            Rectangle {
                required property int index
                width: sprite.r * 0.42
                height: sprite.r * 0.7
                radius: width / 2
                rotation: index === 0 ? -22 : 22
                color: Qt.darker(sprite.fill, 1.6)
                x: sprite.cx + (index === 0 ? -sprite.r * 0.62 : sprite.r * 0.62) - width / 2
                y: sprite.cy - sprite.r * 0.95
            }
        }

        // Hunter antennae with glowing tips.
        Repeater {
            model: sprite.kind === "hunter" ? 2 : 0
            Item {
                required property int index
                x: sprite.cx + (index === 0 ? -sprite.r * 0.4 : sprite.r * 0.4)
                y: sprite.cy - sprite.r
                Rectangle {
                    width: Math.max(1, sprite.cell * 0.03); height: sprite.r * 0.6
                    color: Qt.darker(sprite.fill, 1.5)
                    rotation: index === 0 ? -12 : 12
                }
                Rectangle {
                    width: sprite.r * 0.26; height: width; radius: width / 2
                    color: Qt.lighter(sprite.fill, 1.6)
                    x: -width / 2 + (index === 0 ? -sprite.r * 0.06 : sprite.r * 0.06)
                    y: -height * 0.4
                }
            }
        }

        // Body. A rounded blob, slightly taller than wide and squashed by breathing;
        // bouncers are a glossy round ball instead.
        Rectangle {
            id: body
            width: sprite.r * 2 * (1 + sprite._breath * 0.06 - sprite._hop * 0.08)
            height: sprite.r * 2 * (sprite.isBouncer ? 1.0 : 1.12)
                    * (1 - sprite._breath * 0.06 + sprite._hop * 0.1)
            radius: sprite.isBouncer ? width / 2 : sprite.r * 0.7
            x: sprite.cx - width / 2
            y: sprite.cy - height / 2
            border.color: Qt.darker(sprite.fill, sprite.isGhost ? 1.4 : 2.2)
            border.width: Math.max(1, sprite.cell * 0.03)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(sprite.fill, 1.45) }
                GradientStop { position: 0.5; color: sprite.fill }
                GradientStop { position: 1.0; color: Qt.darker(sprite.fill, 1.35) }
            }

            // Glossy spot, strongest on the bouncer ball.
            Rectangle {
                width: parent.width * 0.34
                height: parent.height * 0.3
                radius: width / 2
                x: parent.width * 0.2
                y: parent.height * 0.16
                color: "#ffffff"
                opacity: sprite.isBouncer ? 0.5 : 0.2
            }

            // Ghost scalloped tail: a row of little lobes along the bottom edge.
            Row {
                visible: sprite.isGhost
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -sprite.r * 0.12
                anchors.horizontalCenter: parent.horizontalCenter
                Repeater {
                    model: 4
                    Rectangle {
                        required property int index
                        width: body.width / 4.2
                        height: width
                        radius: width / 2
                        y: (index % 2 === 0 ? 1 : -1) * sprite.r * 0.12
                        color: sprite.fill
                    }
                }
            }
        }

        // Eyes. The hunter is a single cyclops eye; everyone else has a pair. Both
        // glance toward the heading, with a dark pupil nudged a little further.
        Repeater {
            model: sprite.kind === "hunter" ? 1 : 2
            Rectangle {
                required property int index
                readonly property real side: sprite.kind === "hunter"
                    ? 0 : (index === 0 ? -1 : 1)
                width: sprite.r * (sprite.kind === "hunter" ? 0.66 : 0.46)
                height: width
                radius: width / 2
                color: "#fbfbfb"
                x: sprite.cx + side * sprite.r * 0.42 - width / 2 + sprite._glX * sprite.r * 0.1
                y: sprite.cy - sprite.r * 0.1 - height / 2 + sprite._glY * sprite.r * 0.1

                Rectangle {
                    width: parent.width * 0.5
                    height: width
                    radius: width / 2
                    color: "#16100f"
                    x: parent.width / 2 - width / 2 + sprite._glX * parent.width * 0.26
                    y: parent.height / 2 - height / 2 + sprite._glY * parent.height * 0.26
                }
            }
        }

        // Angry brows on the grounded fighters to read as hostile.
        Repeater {
            model: (sprite.kind === "chaser" || sprite.kind === "wanderer") ? 2 : 0
            Rectangle {
                required property int index
                width: sprite.r * 0.5
                height: Math.max(1, sprite.cell * 0.045)
                radius: height / 2
                color: Qt.darker(sprite.fill, 2.4)
                rotation: index === 0 ? 18 : -18
                x: sprite.cx + (index === 0 ? -sprite.r * 0.5 : sprite.r * 0.5) - width / 2
                y: sprite.cy - sprite.r * 0.42
            }
        }
    }
}
