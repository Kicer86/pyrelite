import QtQuick
import PyreliteApp

Item {
    id: audio

    property var board
    property bool active: true

    property bool initialized: false
    property int lastBombCount: 0
    property int lastExplosionCount: 0
    property int lastPowerUpCount: 0
    property int lastEnemyCount: 0
    property int lastLevel: 1
    property int lastPerkCrystalCount: 0
    property int lastState: BoardModel.Playing
    property int lastTileX: 0
    property int lastTileY: 0
    property double lastStepAt: 0

    visible: false

    function play(source) {
        if (!active)
            return
        player.play(source)
    }

    function syncSnapshot() {
        if (!board) {
            initialized = false
            return
        }

        lastBombCount = board.bombCount
        lastExplosionCount = board.explosionCount
        lastPowerUpCount = board.powerUpCount
        lastEnemyCount = board.enemyCount
        lastLevel = board.level
        lastPerkCrystalCount = board.perkCrystalCount
        lastState = board.state
        lastTileX = Math.round(board.playerX)
        lastTileY = Math.round(board.playerY)
        initialized = true
    }

    function handleBoardChanged() {
        if (!board)
            return
        if (!initialized) {
            syncSnapshot()
            return
        }
        if (!active) {
            syncSnapshot()
            return
        }

        const currentState = board.state
        if (lastState !== BoardModel.Playing) {
            syncSnapshot()
            return
        }
        if (currentState !== BoardModel.Playing) {
            if (currentState === BoardModel.Lost)
                play(gameOverSound)
            syncSnapshot()
            return
        }

        const currentBombCount = board.bombCount
        const currentExplosionCount = board.explosionCount
        const currentPowerUpCount = board.powerUpCount
        const currentEnemyCount = board.enemyCount
        const currentLevel = board.level
        const currentPerkCrystalCount = board.perkCrystalCount

        if (currentBombCount > lastBombCount)
            play(bombPlaceSound)
        if (currentExplosionCount > lastExplosionCount)
            play(explosionSound)
        if (currentEnemyCount < lastEnemyCount)
            play(enemyDownSound)
        if (currentLevel > lastLevel)
            play(levelUpSound)
        else if (currentPowerUpCount < lastPowerUpCount
                 || currentPerkCrystalCount < lastPerkCrystalCount)
            play(pickupSound)

        const currentTileX = Math.round(board.playerX)
        const currentTileY = Math.round(board.playerY)
        const now = Date.now()
        if ((currentTileX !== lastTileX || currentTileY !== lastTileY)
                && now - lastStepAt > 140) {
            play(stepSound)
            lastStepAt = now
        }

        syncSnapshot()
    }

    function preloadSounds() {
        player.preload(stepSound)
        player.preload(bombPlaceSound)
        player.preload(explosionSound)
        player.preload(pickupSound)
        player.preload(enemyDownSound)
        player.preload(levelUpSound)
        player.preload(gameOverSound)
        if (active)
            player.warmUp()
    }

    Component.onCompleted: {
        syncSnapshot()
        preloadSounds()
    }
    onActiveChanged: {
        if (active)
            player.warmUp()
    }
    onBoardChanged: syncSnapshot()

    Connections {
        target: board
        function onChanged() { audio.handleBoardChanged() }
    }

    SoundPlayer { id: player }

    readonly property url stepSound: Qt.resolvedUrl("sounds/step.sfx")
    readonly property url bombPlaceSound: Qt.resolvedUrl("sounds/bomb_place.sfx")
    readonly property url explosionSound: Qt.resolvedUrl("sounds/explosion.sfx")
    readonly property url pickupSound: Qt.resolvedUrl("sounds/pickup.sfx")
    readonly property url enemyDownSound: Qt.resolvedUrl("sounds/enemy_down.sfx")
    readonly property url levelUpSound: Qt.resolvedUrl("sounds/level_up.sfx")
    readonly property url gameOverSound: Qt.resolvedUrl("sounds/game_over.sfx")
}
