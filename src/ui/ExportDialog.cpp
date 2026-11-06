#include "cvatnative/ui/ExportDialog.h"

#include "cvatnative/api/AnnotationsApi.h"
#include "cvatnative/api/MediaApi.h"
#include "cvatnative/api/ServerApi.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QRadioButton>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace cvat {

ExportDialog::ExportDialog(AnnotationsApi* annotations, MediaApi* media, ServerApi* server,
                           const Job& job, QWidget* parent)
    : QDialog(parent), annotations_(annotations), media_(media), server_(server), job_(job)
{
    setWindowTitle(QStringLiteral("Export job %1").arg(job.id));
    resize(560, 340);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    annotationsRadio_ = new QRadioButton(QStringLiteral("Annotations only"), this);
    datasetRadio_ = new QRadioButton(QStringLiteral("Annotations + images (dataset)"), this);
    annotationsRadio_->setToolTip(QStringLiteral("Export annotation data only"));
    datasetRadio_->setToolTip(QStringLiteral("Export annotations together with images"));
    annotationsRadio_->setChecked(true);
    auto* modeRow = new QHBoxLayout;
    auto* annotationsIcon = new QLabel(this);
    annotationsIcon->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(16, 16));
    annotationsIcon->setAccessibleName(QStringLiteral("Annotation file"));
    auto* datasetIcon = new QLabel(this);
    datasetIcon->setPixmap(style()->standardIcon(QStyle::SP_DirIcon).pixmap(16, 16));
    datasetIcon->setAccessibleName(QStringLiteral("Dataset archive"));
    modeRow->addWidget(annotationsIcon);
    modeRow->addWidget(annotationsRadio_);
    modeRow->addWidget(datasetIcon);
    modeRow->addWidget(datasetRadio_);
    modeRow->addStretch();
    form->addRow(QStringLiteral("Export"), modeRow);

    formatCombo_ = new QComboBox(this);
    form->addRow(QStringLiteral("Format"), formatCombo_);

    pathEdit_ = new QLineEdit(this);
    auto* browse = new QPushButton(this);
    browse->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    browse->setToolTip(QStringLiteral("Choose export file"));
    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browse);
    form->addRow(QStringLiteral("File"), pathRow);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);
    layout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    exportButton_ = buttons->addButton(QStringLiteral("Export"), QDialogButtonBox::ActionRole);
    exportButton_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save export"), pathEdit_->text());
        if (!path.isEmpty()) {
            pathEdit_->setText(path);
        }
    });
    connect(exportButton_, &QPushButton::clicked, this, &ExportDialog::exportNow);
    connect(annotationsRadio_, &QRadioButton::toggled, this, [this]() { updateMode(); });
    connect(datasetRadio_, &QRadioButton::toggled, this, [this]() { updateMode(); });

    loadFormats();
    updateMode();
}

void ExportDialog::loadFormats()
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString base = documents.isEmpty() ? QStandardPaths::writableLocation(
                                                  QStandardPaths::HomeLocation)
                                             : documents;
    pathEdit_->setText(base + QStringLiteral("/cvat-job-%1-annotations.zip").arg(job_.id));

    if (!server_) {
        formats_.push_back(FormatInfo{QStringLiteral("CVAT for images 1.1"),
                                      QStringLiteral(".zip"), {}, true,
                                      QStringLiteral("2D")});
        formatCombo_->addItem(formats_.last().name);
        return;
    }

    server_->formats([this](Result<Formats> result) {
        if (!result) {
            statusLabel_->setText(QStringLiteral("Could not load formats: ") +
                                  result.error().toString());
            formats_.push_back(FormatInfo{QStringLiteral("CVAT for images 1.1"),
                                          QStringLiteral(".zip"), {}, true,
                                          QStringLiteral("2D")});
            formatCombo_->addItem(formats_.last().name);
            return;
        }
        formatCombo_->clear();
        formats_.clear();
        for (const auto& format : result->exporters) {
            if (!format.enabled) {
                continue;
            }
            if (!job_.dimension.isEmpty() && !format.dimension.isEmpty() &&
                format.dimension.compare(job_.dimension, Qt::CaseInsensitive) != 0) {
                continue;
            }
            formats_.push_back(format);
            formatCombo_->addItem(format.name);
        }
        if (formats_.isEmpty()) {
            formats_.push_back(FormatInfo{QStringLiteral("CVAT for images 1.1"),
                                          QStringLiteral(".zip"), {}, true,
                                          QStringLiteral("2D")});
            formatCombo_->addItem(formats_.last().name);
        }
    });
}

void ExportDialog::updateMode()
{
    const bool dataset = datasetRadio_->isChecked();
    statusLabel_->setText(dataset
        ? QStringLiteral("A dataset archive embeds annotations and media; this can be large.")
        : QStringLiteral("Only the annotation file is downloaded."));
}

void ExportDialog::exportNow()
{
    const QString path = pathEdit_->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Choose an output file."));
        return;
    }
    if (formats_.isEmpty() || formatCombo_->currentIndex() < 0) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("No export format is selected."));
        return;
    }
    const QString format = formats_[formatCombo_->currentIndex()].name;
    const bool dataset = datasetRadio_->isChecked();
    exportButton_->setEnabled(false);
    statusLabel_->setText(QStringLiteral("Exporting…"));

    const QPointer<ExportDialog> dialog(this);
    Finish finish = [dialog, path](Result<QByteArray> result) {
        if (!dialog) {
            return;
        }
        dialog->exportButton_->setEnabled(true);
        if (!result) {
            dialog->statusLabel_->setText(QStringLiteral("Export failed: ") +
                                          result.error().toString());
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            dialog->statusLabel_->setText(QStringLiteral("Cannot write: ") + path);
            return;
        }
        if (file.write(*result) != result->size() || !file.flush()) {
            dialog->statusLabel_->setText(
                QStringLiteral("Could not write the complete export."));
            return;
        }
        file.close();
        dialog->statusLabel_->setText(
            QStringLiteral("Saved %1 (%2 bytes)").arg(path).arg(result->size()));
        QMessageBox::information(dialog, QStringLiteral("Export"),
                                 QStringLiteral("Export saved to:\n") + path);
    };

    if (!dataset) {
        annotations_->exportAnnotations(job_.id, format, finish);
        return;
    }

    statusLabel_->setText(QStringLiteral("Starting dataset export…"));
    media_->startJobDatasetExport(job_.id, format, true,
                                  [dialog, finish](Result<QString> requestId) {
        if (!dialog) {
            return;
        }
        if (!requestId) {
            finish(requestId.error());
            return;
        }
        dialog->pollDataset(*requestId, 0, finish);
    });
}

void ExportDialog::pollDataset(const QString& requestId, int attempt, Finish finish)
{
    constexpr int maxAttempts = 900;
    if (attempt >= maxAttempts) {
        finish(ApiError(ErrorKind::Timeout,
                        QStringLiteral("Dataset export timed out after 900 attempts")));
        return;
    }

    const QPointer<ExportDialog> dialog(this);
    server_->request(requestId, [dialog, requestId, attempt, finish](
                                      Result<RequestState> result) {
        if (!dialog) {
            return;
        }
        if (!result) {
            finish(result.error());
            return;
        }
        if (result->failed()) {
            finish(ApiError(ErrorKind::Server,
                            result->message.isEmpty()
                                ? QStringLiteral("CVAT dataset export failed")
                                : result->message));
            return;
        }
        if (!result->finished()) {
            dialog->statusLabel_->setText(QStringLiteral("Exporting… %1%")
                                              .arg(static_cast<int>(result->progress)));
            QTimer::singleShot(1000, dialog, [dialog, requestId, attempt, finish]() {
                if (dialog) {
                    dialog->pollDataset(requestId, attempt + 1, finish);
                }
            });
            return;
        }
        dialog->statusLabel_->setText(QStringLiteral("Downloading exported dataset…"));
        dialog->media_->downloadResultUrl(result->resultUrl, finish);
    });
}

} // namespace cvat
