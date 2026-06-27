
import QtQuick

// A level-up perk crystal lying on the floor: a faceted gem that floats, shimmers
// and glows gold, with its perk name above. Bigger and more lustrous than a brick
// power-up so it reads as "special" loot. The model supplies the fill and the
// label; presentation lives here alone.
Item {
    id: sprite

    property real cell: 48
    property color fill: "#ffcf40"
    property string label: ""

    width: cell
    height: cell

    readonly property real cx: cell / 2
    readonly property real cy: cell / 2
    readonly property real s: cell * 0.27   // crystal half-diagonal

    // Float and shimmer: a slow vertical drift plus a breathing scale, so the gem
    // feels alive and draws the eye under pressure.
    property real _bob: 0
    SequentialAnimation on _bob {
        running: true
        loops: Animation.Infinite
        NumberAnimation { from: -1; to: 1; duration: 1100; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: -1; duration: 1100; easing.type: Easing.InOutSine }
    }
    property real _shimmer: 1
    SequentialAnimation on _shimmer {
        running: true
        loops: Animation.Infinite
        NumberAnimation { from: 0.9; to: 1.1; duration: 650; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.1; to: 0.9; duration: 650; easing.type: Easing.InOutSine }
    }

    // Gold glow halo behind the gem, pulsing with the shimmer.
    Rectangle {
        width: sprite.s * 3.6
        height: width
        radius: width / 2
        x: sprite.cx - width / 2
        y: sprite.cy - height / 2 + sprite._bob * sprite.cell * 0.05
        color: "#ffe9a0"
        opacity: 0.12 + (sprite._shimmer - 0.9) * 0.5
    }

    // The gem itself: a diamond split into top and bottom facets, gold-rimmed,
    // floating and shimmering as one.
    Item {
        x: sprite.cx
        y: sprite.cy + sprite._bob * sprite.cell * 0.06
        scale: sprite._shimmer

        // Lower (darker) facet.
        Rectangle {
            width: sprite.s * 2; height: width
            radius: sprite.cell * 0.05
            rotation: 45
            x: -width / 2; y: -height / 2
            color: Qt.darker(sprite.fill, 1.4)
            border.color: "#fff1c0"
            border.width: Math.max(1, sprite.cell * 0.05)
        }
        // Upper (lit) facet: a half-height bright cap clipped to the diamond's top.
        Rectangle {
            width: sprite.s * 1.42; height: width
            radius: sprite.cell * 0.04
            rotation: 45
            x: -width / 2; y: -sprite.s * 0.5 - height / 2 + sprite.s * 0.5
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(sprite.fill, 1.6) }
                GradientStop { position: 1.0; color: sprite.fill }
            }
            opacity: 0.96
        }
        // Sharp glint near the top facet.
        Rectangle {
            width: sprite.s * 0.34; height: width; radius: width / 2
            color: "#fffdf2"
            x: -sprite.s * 0.42; y: -sprite.s * 0.42
            opacity: 0.85
        }
    }

    // Perk name floating above the gem.
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 1
        text: sprite.label
        color: "#fff1c0"
        font.pixelSize: 11
        font.bold: true
        style: Text.Outline
        styleColor: "#000000"
    }
}
