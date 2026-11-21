#pragma once

#include <QColor>
#include <QIcon>
#include <QRectF>

class QPainter;

// Simple line icons drawn in code (Fluent-like, 1.5px strokes), so the app
// needs no icon font or image files.
namespace Icons {

enum class Icon {
    Home,      // house
    Training,  // presentation board
    PreMeet,   // checklist
    Meet,      // video camera
    PostMeet,  // bar chart
    Info,
    Add,
    Close,
    Play,
    Next,
    Stop,
    Eye,
    Settings,  // gear
    Camera,
    Mic,
    Sparkle,   // AI
    ArrowUp,
    ArrowDown,
    ChevronDown,
    ChevronRight,
    Bug,       // debug
    Text,      // paragraph
    Bullets,   // bulleted list
};

void paint(QPainter &painter, const QRectF &rect, Icon icon, const QColor &color);
QIcon icon(Icon icon, const QColor &color, int size = 16);

} // namespace Icons
