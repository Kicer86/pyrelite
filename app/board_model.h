
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "grid.h"

// Thin QML-facing adapter over a core arena grid. Owns the generated board and
// exposes its size + per-cell tile to QML. (Generation is deterministic; the
// run seed will be wired in later.)
class BoardModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)

public:
    enum Tile { Empty, Wall, Brick };
    Q_ENUM(Tile)

    explicit BoardModel(QObject *parent = nullptr);

    int columns() const { return grid_.width(); }
    int rows() const { return grid_.height(); }

    Q_INVOKABLE int tileAt(int x, int y) const;

private:
    pyrelite::Grid grid_;
};
