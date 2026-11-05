#pragma once

#include "cvatnative/api/Models.h"
#include "cvatnative/model/Annotation.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace cvat {

// Label chooser plus a generated attribute editor for the selected object.
class LabelPanel : public QWidget {
    Q_OBJECT
public:
    explicit LabelPanel(QWidget* parent = nullptr);

    void setLabels(const QVector<Label>& labels);
    void setActiveLabel(int labelId);
    [[nodiscard]] int activeLabel() const { return activeLabel_; }

    void showObject(const QString& key, int labelId,
                    const QVector<AttributeValue>& attributes, bool occluded,
                    const QString& summary);
    void clearObject();

signals:
    void labelActivated(int labelId);
    void attributesChanged(const QVector<AttributeValue>& attributes);
    void occludedChanged(bool occluded);
    void deleteRequested();

private:
    struct AttributeEditor {
        int specId = 0;
        QString inputType;
        QWidget* widget = nullptr;
    };

    void rebuildAttributes(const QVector<AttributeValue>& attributes);
    void emitAttributes();
    [[nodiscard]] const Label* findLabel(int id) const;

    QListWidget* labelList_ = nullptr;
    QLabel* objectTitle_ = nullptr;
    QCheckBox* occludedBox_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QWidget* attributeHost_ = nullptr;
    QFormLayout* attributeLayout_ = nullptr;
    QVector<Label> labels_;
    QVector<AttributeEditor> editors_;
    QString objectKey_;
    int activeLabel_ = 0;
    int objectLabelId_ = 0;
    bool updating_ = false;
};

} // namespace cvat
