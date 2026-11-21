#pragma once

#include <QWidget>

#include "core/Agenda.h"

class QVBoxLayout;

// Editable list of agenda points: number, title, minutes, remove button.
class AgendaEditor : public QWidget
{
    Q_OBJECT

public:
    explicit AgendaEditor(QWidget *parent = nullptr);

    Agenda agenda() const;
    void addPoint(const QString &title, double minutes, bool focus = false);

signals:
    void changed();

private:
    void renumber();

    QVBoxLayout *m_rows;
};
