#pragma once

#include "cvatnative/api/Models.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QPushButton;

namespace cvat {

class AnnotationsApi;
class MediaApi;
class ServerApi;

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(AnnotationsApi* annotations, MediaApi* media, ServerApi* server,
                 const Job& job, QWidget* parent = nullptr);

private:
    void loadFormats();
    void exportNow();
    void updateMode();

    AnnotationsApi* annotations_;
    MediaApi* media_;
    ServerApi* server_;
    Job job_;

    QComboBox* formatCombo_ = nullptr;
    QRadioButton* annotationsRadio_ = nullptr;
    QRadioButton* datasetRadio_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QVector<FormatInfo> formats_;
};

} // namespace cvat
