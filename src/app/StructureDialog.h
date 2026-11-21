#pragma once

#include <QDialog>

#include "core/Agenda.h"

class QLabel;
class QVBoxLayout;

// Review the sections SMILE found in your speech: edit titles and text,
// reorder, add or delete, then "Confirm structure". Sections you changed are
// marked "Edited"; untouched ones stay marked "AI".
class StructureDialog : public QDialog
{
    Q_OBJECT

public:
    StructureDialog(const Agenda &sections, const QString &presentationType, bool fromAi, QWidget *parent = nullptr);

    Agenda sections() const;

private:
    void addSection(const AgendaPoint &point, bool generated);
    void renumber();
    void move(QWidget *card, int delta);

    QVBoxLayout *m_list;
    QLabel *m_summary;
};
