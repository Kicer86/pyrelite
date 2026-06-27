
import QtQuick
import PyreliteApp

// A brick-drop power-up: a floating gem token with a pulsing halo and a white
// glyph naming its boon. The model reports the kind; this is the single place the
// power-up's look lives, so adding a kind adds one branch here and nothing in core.
Item {
    id: sprite

    property real cell: 48
    property int kind: BoardModel.BombLimitPowerUp

    width: cell
    height: cell

    readonly property real cx: cell / 2
    readonly property real cy: cell / 2
    readonly property real s: cell * 0.21   // token half-diagonal

    readonly property color tint: kind === BoardModel.BombLimitPowerUp ? "#2fb8ac"
                                : kind === BoardModel.BombRangePowerUp ? "#4f8cff"
                                : "#d85ce6"

    // Gentle float and a breathing halo, so drops catch the eye on the floor.
    property real _bob: 0
    SequentialAnimation on _bob {
        running: true
        loops: Animation.Infinite
        NumberAnimation { from: -1; to: 1; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: -1; duration: 900; easing.type: Easing.InOutSine }
    }

    // Soft glow halo behind the token.
    Rectangle {
        width: sprite.s * 3.2
        height: width
        radius: width / 2
        x: sprite.cx - width / 2
        y: sprite.cy - height / 2 + sprite._bob * sprite.cell * 0.04
        color: sprite.tint
        opacity: 0.18 + (sprite._bob + 1) * 0.06
    }

    Item {
        x: sprite.cx
        y: sprite.cy + sprite._bob * sprite.cell * 0.05

        // Diamond token: a rotated rounded square with a top-lit facet.
        Rectangle {
            width: sprite.s * 2
            height: width
            radius: sprite.cell * 0.07
            rotation: 45
            x: -width / 2
            y: -height / 2
            border.color: "#0e0e12"
            border.width: Math.max(1, sprite.cell * 0.04)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(sprite.tint, 1.5) }
                GradientStop { position: 1.0; color: Qt.darker(sprite.tint, 1.3) }
            }
        }

        // Upright glyph layer (not rotated with the diamond).
        // Bomb-limit: a little bomb with a "+" badge.
        Item {
            visible: sprite.kind === BoardModel.BombLimitPowerUp
            Rectangle {
                width: sprite.s * 0.8; height: width; radius: width / 2
                x: -width / 2; y: -height / 2 + sprite.s * 0.18
                color: "#101014"
            }
            Rectangle { // fuse
                width: Math.max(1, sprite.cell * 0.035); height: sprite.s * 0.4
                radius: width / 2; color: "#101014"; rotation: 20
                x: sprite.s * 0.18; y: -sprite.s * 0.5
            }
            Rectangle { // + bar h
                width: sprite.s * 0.62; height: Math.max(1, sprite.cell * 0.06)
                radius: height / 2; color: "#ffffff"
                x: -width / 2; y: sprite.s * 0.12 - height / 2
            }
            Rectangle { // + bar v
                width: Math.max(1, sprite.cell * 0.06); height: sprite.s * 0.62
                radius: width / 2; color: "#ffffff"
                x: -width / 2; y: sprite.s * 0.12 - height / 2
            }
        }

        // Bomb-range: a four-way burst — bars with diamond arrowheads.
        Repeater {
            model: sprite.kind === BoardModel.BombRangePowerUp ? 4 : 0
            Item {
                required property int index
                rotation: index * 90
                Rectangle {
                    width: Math.max(1, sprite.cell * 0.06); height: sprite.s * 0.95
                    radius: width / 2; color: "#ffffff"
                    x: -width / 2; y: -height
                }
                Rectangle { // arrowhead
                    width: sprite.s * 0.32; height: width; radius: sprite.cell * 0.02
                    rotation: 45; color: "#ffffff"
                    x: -width / 2; y: -sprite.s * 0.95 - height * 0.35
                }
            }
        }

        // Speed: stacked chevrons pointing up.
        Repeater {
            model: sprite.kind === BoardModel.SpeedPowerUp ? 2 : 0
            Item {
                required property int index
                y: index * sprite.s * 0.5 - sprite.s * 0.25
                Rectangle {
                    width: sprite.s * 0.78; height: Math.max(1, sprite.cell * 0.06)
                    radius: height / 2; color: "#ffffff"; rotation: -38
                    transformOrigin: Item.Right
                    x: -width; y: 0
                }
                Rectangle {
                    width: sprite.s * 0.78; height: Math.max(1, sprite.cell * 0.06)
                    radius: height / 2; color: "#ffffff"; rotation: 38
                    transformOrigin: Item.Left
                    x: 0; y: 0
                }
            }
        }
    }
}
