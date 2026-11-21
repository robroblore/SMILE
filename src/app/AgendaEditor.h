#pragma once

#include <QWidget>

#include "core/Agenda.h"

class QVBoxLayout;

// Editable list of sections: rename, reorder, set a time, add/remove, and
// expand a section to edit its purpose and key points.
class AgendaEditor : public QWidget
{
    Q_OBJECT

public:
    explicit AgendaEditor(QWidget *parent = nullptr);

    Agenda agenda() const;
    void setAgenda(const Agenda &agenda);
    void addPoint(const QString &title, double minutes, bool focus = false);

signals:
    void changed();

private:
    void addSection(const AgendaPoint &point, bool focus, bool expanded);
    void move(QWidget *row, int delta);
    void renumber();

    QVBoxLayout *m_rows;
};
