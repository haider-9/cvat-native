#pragma once

#include "cvatnative/api/Models.h"

#include <QDialog>
#include <QJsonArray>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTableWidget;

namespace cvat {

class ProjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProjectDialog(QWidget* parent = nullptr);

    [[nodiscard]] QString projectName() const;
    [[nodiscard]] QJsonArray labels() const;

private:
    QLineEdit* nameEdit_ = nullptr;
    QTableWidget* labelsTable_ = nullptr;
};

class TaskDialog : public QDialog {
    Q_OBJECT
public:
    TaskDialog(const QVector<Project>& projects, int defaultProjectId,
               QWidget* parent = nullptr);

    [[nodiscard]] QString taskName() const;
    [[nodiscard]] int projectId() const;
    [[nodiscard]] int segmentSize() const;
    [[nodiscard]] int overlap() const;
    [[nodiscard]] int imageQuality() const;
    [[nodiscard]] QString subset() const;
    [[nodiscard]] QJsonArray labels() const;
    [[nodiscard]] QStringList files() const;

private:
    void updateLabelState();

    QLineEdit* nameEdit_ = nullptr;
    QComboBox* projectCombo_ = nullptr;
    QSpinBox* segmentSpin_ = nullptr;
    QSpinBox* overlapSpin_ = nullptr;
    QSpinBox* qualitySpin_ = nullptr;
    QLineEdit* subsetEdit_ = nullptr;
    QTableWidget* labelsTable_ = nullptr;
    QListWidget* filesList_ = nullptr;
};

} // namespace cvat
