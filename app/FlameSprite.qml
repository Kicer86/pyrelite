
import QtQuick

// A blast flame on one tile (no art assets yet): concentric heat layers from a
// white-hot core out to a red glow, ringed by flickering tongues. The model feeds
// `life` (1 at ignition to 0 as it dies); the flame bursts in, holds, then fades on
// that clock, while the per-layer flicker loops independently so it never freezes.
Item {
    id: sprite

    property real cell: 48
    // Fraction of flame life remaining, 1 fresh down to 0 as it dies.
    property real life: 1

    width: cell
    height: cell

    readonly property real cx: cell / 2
    readonly property real cy: cell / 2
    readonly property real age: 1 - life

    // Envelope: a fast pop to full size, then a slow shrink as it burns out.
    readonly property real env: age < 0.2
        ? 0.5 + (age / 0.2) * 0.6
        : Math.max(0.7, 1.1 - (age - 0.2) / 0.8 * 0.4)
    // Hold opaque, then fade over the final third of the life.
    readonly property real fade: age < 0.66 ? 1 : Math.max(0, 1 - (age - 0.66) / 0.34)

    opacity: fade

    // Heat layers from the outside in. Each flickers on its own short loop.
    Repeater {
        model: [
            { d: 0.98, color: "#c81e0a" },
            { d: 0.74, color: "#ff7a16" },
            { d: 0.5,  color: "#ffc636" },
            { d: 0.28, color: "#fff4cf" }
        ]
        Rectangle {
            required property int index
            required property var modelData
            readonly property real base: sprite.cell * modelData.d * sprite.env

            width: base
            height: base
            radius: width / 2
            color: modelData.color
            x: sprite.cx - width / 2
            y: sprite.cy - height / 2

            SequentialAnimation on scale {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 0.92; to: 1.08; duration: 90 + index * 25; easing.type: Easing.InOutSine }
                NumberAnimation { from: 1.08; to: 0.92; duration: 90 + index * 25; easing.type: Easing.InOutSine }
            }
        }
    }

    // Flame tongues licking outward, flickering in length around the rim.
    Repeater {
        model: 6
        Item {
            required property int index
            x: sprite.cx
            y: sprite.cy
            rotation: index * 60

            Rectangle {
                id: tongue
                width: sprite.cell * 0.16
                height: sprite.cell * 0.34 * sprite.env
                radius: width / 2
                x: -width / 2
                y: -sprite.cell * 0.34 * sprite.env - sprite.cell * 0.16
                color: "#ff9326"

                SequentialAnimation on scale {
                    running: true
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.6; to: 1.15; duration: 110 + (index % 3) * 40; easing.type: Easing.OutQuad }
                    NumberAnimation { from: 1.15; to: 0.6; duration: 120 + (index % 3) * 40; easing.type: Easing.InQuad }
                }
            }
        }
    }
}
