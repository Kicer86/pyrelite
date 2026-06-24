
import QtQuick

// A lit bomb (no art assets yet): a metallic sphere faked from a dark base, a
// top-left highlight and a rim, capped by a neck, a fuse and a sputtering spark.
// As the fuse burns down the model feeds `fuse` from 1 to 0; the sphere reddens
// and its heartbeat pulse tightens, so the blast is telegraphed before it lands.
Item {
    id: sprite

    property real cell: 48
    // Fraction of fuse remaining, 1 just-placed down to 0 at detonation.
    property real fuse: 1

    width: cell
    height: cell

    // 0 when freshly placed, 1 at the brink. Drives reddening and pulse speed.
    readonly property real danger: 1 - fuse
    readonly property real cx: cell / 2
    readonly property real cy: cell / 2
    readonly property real r: cell * 0.3

    // Body tint: near-black drifting toward a hot maroon as detonation nears.
    readonly property color bodyColor: Qt.rgba(0.12 + danger * 0.5,
                                               0.12 - danger * 0.05,
                                               0.13 - danger * 0.05, 1)

    // Heartbeat pulse. The loop's duration shrinks with danger, so the throb speeds
    // up; amplitude grows too. In the last breath it blinks fast — the final warning.
    property real _pulse: 0
    SequentialAnimation on _pulse {
        running: true
        loops: Animation.Infinite
        NumberAnimation {
            from: 0; to: 1
            duration: Math.max(90, 520 - sprite.danger * 400)
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            from: 1; to: 0
            duration: Math.max(90, 520 - sprite.danger * 400)
            easing.type: Easing.InQuad
        }
    }
    readonly property real _scale: 1 + _pulse * (0.06 + danger * 0.18)

    // Contact shadow under the bomb.
    Rectangle {
        width: sprite.r * 2.1
        height: sprite.r * 0.7
        radius: height / 2
        color: "#000000"
        opacity: 0.3
        x: sprite.cx - width / 2
        y: sprite.cy + sprite.r * 0.62
    }

    // Sphere, pulsing about its centre.
    Item {
        x: sprite.cx
        y: sprite.cy
        scale: sprite._scale

        Rectangle {
            width: sprite.r * 2
            height: width
            radius: width / 2
            x: -width / 2
            y: -height / 2
            color: sprite.bodyColor
            border.color: Qt.lighter(sprite.bodyColor, 1.4)
            border.width: Math.max(1, sprite.cell * 0.02)

            // Broad soft highlight to round the top-left into a sphere.
            Rectangle {
                width: parent.width * 0.5
                height: width
                radius: width / 2
                x: parent.width * 0.14
                y: parent.height * 0.12
                color: "#ffffff"
                opacity: 0.22
            }
            // Tight specular dot.
            Rectangle {
                width: parent.width * 0.16
                height: width
                radius: width / 2
                x: parent.width * 0.26
                y: parent.height * 0.2
                color: "#ffffff"
                opacity: 0.6
            }
            // Danger glow welling up from within as the fuse runs out.
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.7
                height: width
                radius: width / 2
                color: "#ff5a2a"
                opacity: sprite.danger * 0.5 * (0.5 + sprite._pulse * 0.5)
            }
        }
    }

    // Neck cap where the fuse seats.
    Rectangle {
        width: sprite.r * 0.7
        height: sprite.r * 0.45
        radius: sprite.cell * 0.04
        color: "#3a3a40"
        border.color: "#15151a"
        border.width: Math.max(1, sprite.cell * 0.02)
        x: sprite.cx - width / 2
        y: sprite.cy - sprite.r - height * 0.5
    }

    // Fuse string, leaning out of the neck.
    Rectangle {
        width: Math.max(1, sprite.cell * 0.045)
        height: sprite.r * 0.7
        radius: width / 2
        color: "#caa15a"
        rotation: 18
        transformOrigin: Item.Bottom
        x: sprite.cx - width / 2
        y: sprite.cy - sprite.r - height
    }

    // Sputtering spark at the fuse tip: a flickering glow that brightens with danger.
    Item {
        x: sprite.cx + sprite.r * 0.28
        y: sprite.cy - sprite.r - sprite.r * 0.62

        Rectangle {
            id: spark
            width: sprite.r * (0.34 + sprite.danger * 0.25)
            height: width
            radius: width / 2
            x: -width / 2
            y: -height / 2
            color: "#ffd23a"

            SequentialAnimation on opacity {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 0.55; to: 1.0; duration: 70 }
                NumberAnimation { from: 1.0; to: 0.6; duration: 90 }
                NumberAnimation { from: 0.6; to: 0.9; duration: 60 }
            }
            SequentialAnimation on scale {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 0.8; to: 1.2; duration: 80; easing.type: Easing.OutQuad }
                NumberAnimation { from: 1.2; to: 0.85; duration: 100; easing.type: Easing.InQuad }
            }

            // Hot white core.
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.5
                height: width
                radius: width / 2
                color: "#fff6cf"
            }
        }
    }
}
