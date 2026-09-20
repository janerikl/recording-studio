#include "LoopBrowserPanel.h"

#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

namespace rsd {

namespace {

constexpr auto kSettingsKey = "loopBrowserFolder";

// Only override needed vs. plain QListWidget: outgoing drags carry a
// standard file URL (QMimeData::setUrls), not a custom MIME type, so any
// existing file drop target (ClipLaneWidget, MediaLibraryPanel, an outside
// app) handles it with no new plumbing.
class DragListWidget : public QListWidget {
public:
    explicit DragListWidget(QWidget* parent) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        if (items.isEmpty()) return nullptr;
        QString path = items.first()->data(Qt::UserRole).toString();
        if (path.isEmpty()) return nullptr;

        auto* mime = new QMimeData();
        mime->setUrls({QUrl::fromLocalFile(path)});
        return mime;
    }
};

} // namespace

LoopBrowserPanel::LoopBrowserPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);

    auto* folderRow = new QHBoxLayout();
    m_folderLabel = new QLabel("No folder chosen", this);
    m_folderLabel->setWordWrap(true);
    m_chooseFolderButton = new QPushButton("Choose Folder...", this);
    connect(m_chooseFolderButton, &QPushButton::clicked, this, &LoopBrowserPanel::chooseFolder);
    folderRow->addWidget(m_folderLabel, 1);
    folderRow->addWidget(m_chooseFolderButton);
    outer->addLayout(folderRow);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter by name...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &LoopBrowserPanel::applyFilter);
    outer->addWidget(m_filterEdit);

    m_list = new DragListWidget(this);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString path = item->data(Qt::UserRole).toString();
        if (path == m_previewingPath) {
            m_previewingPath.clear();
            emit previewRequested(QString());
        } else {
            m_previewingPath = path;
            emit previewRequested(path);
        }
    });
    outer->addWidget(m_list, 1);

    m_stopButton = new QPushButton("Stop Preview", this);
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        m_previewingPath.clear();
        emit previewRequested(QString());
    });
    outer->addWidget(m_stopButton);

    QSettings settings("RecordingStudio", "RecordingStudio");
    QString savedFolder = settings.value(kSettingsKey).toString();
    if (!savedFolder.isEmpty() && QFileInfo::exists(savedFolder)) {
        m_folderPath = savedFolder;
        m_folderLabel->setText(m_folderPath);
        rescan();
    }
}

void LoopBrowserPanel::chooseFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Choose Sample Folder", m_folderPath);
    if (dir.isEmpty()) return;

    m_folderPath = dir;
    m_folderLabel->setText(m_folderPath);

    QSettings settings("RecordingStudio", "RecordingStudio");
    settings.setValue(kSettingsKey, m_folderPath);

    rescan();
}

void LoopBrowserPanel::rescan() {
    m_allFiles.clear();
    if (m_folderPath.isEmpty()) {
        applyFilter();
        return;
    }

    QStringList nameFilters = {"*.wav", "*.aiff", "*.aif", "*.flac", "*.ogg"};
    QDirIterator it(m_folderPath, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) m_allFiles.append(it.next());
    m_allFiles.sort(Qt::CaseInsensitive);

    applyFilter();
}

void LoopBrowserPanel::applyFilter() {
    m_list->clear();
    QString filter = m_filterEdit->text();

    for (const QString& path : m_allFiles) {
        QString baseName = QFileInfo(path).fileName();
        if (!filter.isEmpty() && !baseName.contains(filter, Qt::CaseInsensitive)) continue;

        auto* item = new QListWidgetItem(baseName);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_list->addItem(item);
    }
}

} // namespace rsd
