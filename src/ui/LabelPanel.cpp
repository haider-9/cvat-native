#include "cvatnative/ui/LabelPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QPalette>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace cvat {

namespace {
QIcon colorIcon(const QString& color)
{
    QPixmap pixmap(12, 12);
    pixmap.fill(QColor(color));
    return QIcon(pixmap);
}
} // namespace

LabelPanel::LabelPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* labelGroup = new QGroupBox(QStringLiteral("Labels"), this);
    auto* labelLayout = new QVBoxLayout(labelGroup);
    labelList_ = new QListWidget(labelGroup);
    labelList_->setAlternatingRowColors(true);
    labelList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    labelList_->setMaximumHeight(180);
    labelLayout->addWidget(labelList_);

    auto* visibilityRow = new QHBoxLayout;
    visibilityRow->setSpacing(6);
    hideButton_ = new QPushButton(QStringLiteral("Hide"), labelGroup);
    hideButton_->setToolTip(QStringLiteral("Hide the selected classes everywhere (objects and tags)"));
    showButton_ = new QPushButton(QStringLiteral("Show"), labelGroup);
    showButton_->setToolTip(QStringLiteral("Reveal the selected classes"));
    showAllButton_ = new QPushButton(QStringLiteral("Show all"), labelGroup);
    showAllButton_->setToolTip(QStringLiteral("Reveal every hidden class"));
    for (auto* button : {hideButton_, showButton_, showAllButton_}) {
        button->setFocusPolicy(Qt::NoFocus);
        visibilityRow->addWidget(button);
    }
    labelLayout->addLayout(visibilityRow);

    auto* shortcutHint = new QLabel(
        QStringLiteral("[ and ] switch label · Ctrl+click selects several classes"), labelGroup);
    shortcutHint->setWordWrap(true);
    shortcutHint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    labelLayout->addWidget(shortcutHint);
    layout->addWidget(labelGroup);

    auto* objectGroup = new QGroupBox(QStringLiteral("Selected object"), this);
    auto* objectLayout = new QVBoxLayout(objectGroup);
    objectTitle_ = new QLabel(QStringLiteral("No selection"), objectGroup);
    objectTitle_->setWordWrap(true);
    objectLayout->addWidget(objectTitle_);

    occludedBox_ = new QCheckBox(QStringLiteral("Occluded"), objectGroup);
    occludedBox_->setEnabled(false);
    objectLayout->addWidget(occludedBox_);

    attributeHost_ = new QWidget(objectGroup);
    attributeLayout_ = new QFormLayout(attributeHost_);
    attributeLayout_->setContentsMargins(0, 0, 0, 0);
    objectLayout->addWidget(attributeHost_);

    deleteButton_ = new QPushButton(QStringLiteral("Delete object"), objectGroup);
    deleteButton_->setEnabled(false);
    objectLayout->addWidget(deleteButton_);

    layout->addWidget(objectGroup, 1);

    connect(labelList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (updating_ || row < 0 || row >= labels_.size()) {
            return;
        }
        activeLabel_ = labels_[row].id;
        emit labelActivated(activeLabel_);
    });
    connect(labelList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (updating_ || !item) {
            return;
        }
        const int row = labelList_->row(item);
        if (row >= 0 && row < labels_.size()) {
            emit labelDoubleClicked(labels_[row].id);
        }
    });
    connect(labelList_, &QListWidget::itemSelectionChanged, this,
            &LabelPanel::updateVisibilityButtons);
    connect(hideButton_, &QPushButton::clicked, this, [this]() {
        emit hideLabelsRequested(selectedLabelIds());
    });
    connect(showButton_, &QPushButton::clicked, this, [this]() {
        emit showLabelsRequested(selectedLabelIds());
    });
    connect(showAllButton_, &QPushButton::clicked, this,
            &LabelPanel::showAllLabelsRequested);
    updateVisibilityButtons();

    connect(occludedBox_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!updating_ && !objectKey_.isEmpty()) {
            emit occludedChanged(checked);
        }
    });
    connect(deleteButton_, &QPushButton::clicked, this, [this]() {
        if (!objectKey_.isEmpty()) {
            emit deleteRequested();
        }
    });
}

void LabelPanel::setLabels(const QVector<Label>& labels)
{
    labels_ = labels;
    updating_ = true;
    labelList_->clear();
    for (const auto& label : labels_) {
        auto* item = new QListWidgetItem(label.name, labelList_);
        item->setData(Qt::UserRole, label.id);
    }
    refreshLabelItems();
    int activeRow = -1;
    for (int i = 0; i < labels_.size(); ++i) {
        if (labels_[i].id == activeLabel_) {
            activeRow = i;
            break;
        }
    }
    if (activeRow < 0 && !labels_.isEmpty() && activeLabel_ == 0) {
        activeLabel_ = labels_.first().id;
        activeRow = 0;
    }
    if (activeRow >= 0) {
        labelList_->setCurrentRow(activeRow);
    }
    updating_ = false;
    updateVisibilityButtons();
    if (activeLabel_ > 0) {
        emit labelActivated(activeLabel_);
    }
}

void LabelPanel::setActiveLabel(int labelId)
{
    activeLabel_ = labelId;
    for (int i = 0; i < labels_.size(); ++i) {
        if (labels_[i].id == labelId) {
            QSignalBlocker blocker(labelList_);
            labelList_->setCurrentRow(i);
            break;
        }
    }
}

void LabelPanel::setHiddenLabels(const QSet<int>& hidden)
{
    hiddenLabels_ = hidden;
    refreshLabelItems();
    updateVisibilityButtons();
}

void LabelPanel::refreshLabelItems()
{
    for (int row = 0; row < labelList_->count(); ++row) {
        QListWidgetItem* item = labelList_->item(row);
        const Label* label = findLabel(item->data(Qt::UserRole).toInt());
        if (!label) {
            continue;
        }
        const bool hidden = hiddenLabels_.contains(label->id);
        item->setIcon(colorIcon(label->color));
        item->setText(hidden ? QStringLiteral("%1 (hidden)").arg(label->name) : label->name);
        item->setForeground(hidden ? QColor(QStringLiteral("#888888"))
                                   : labelList_->palette().color(QPalette::Text));
        QFont font = item->font();
        font.setItalic(hidden);
        item->setFont(font);
        item->setToolTip(hidden
            ? QStringLiteral("%1 (%2) — hidden; select it and press Show").arg(label->name, label->type)
            : QStringLiteral("%1 (%2)").arg(label->name, label->type));
    }
}

QSet<int> LabelPanel::selectedLabelIds() const
{
    QSet<int> ids;
    for (QListWidgetItem* item : labelList_->selectedItems()) {
        const int id = item->data(Qt::UserRole).toInt();
        if (id > 0) {
            ids.insert(id);
        }
    }
    return ids;
}

void LabelPanel::updateVisibilityButtons()
{
    const bool hasSelection = !selectedLabelIds().isEmpty();
    hideButton_->setEnabled(hasSelection);
    showButton_->setEnabled(hasSelection);
    showAllButton_->setEnabled(!hiddenLabels_.isEmpty());
}

const Label* LabelPanel::findLabel(int id) const
{
    for (const auto& label : labels_) {
        if (label.id == id) {
            return &label;
        }
    }
    return nullptr;
}

void LabelPanel::rebuildAttributes(const QVector<AttributeValue>& attributes)
{
    updating_ = true;
    editors_.clear();
    while (attributeLayout_->rowCount() > 0) {
        attributeLayout_->removeRow(0);
    }

    const Label* label = findLabel(objectLabelId_);
    if (!label) {
        updating_ = false;
        return;
    }

    for (const auto& attribute : label->attributes) {
        QString value = attribute.defaultValue;
        if (value.isEmpty() && attribute.inputType == QStringLiteral("select") &&
            !attribute.values.isEmpty()) {
            value = attribute.values.first();
        }
        for (const auto& current : attributes) {
            if (current.specId == attribute.id) {
                value = current.value;
                break;
            }
        }

        QWidget* editor = nullptr;
        AttributeEditor entry;
        entry.specId = attribute.id;
        entry.inputType = attribute.inputType;

        if (attribute.inputType == QStringLiteral("select") ||
            attribute.inputType == QStringLiteral("radio")) {
            auto* combo = new QComboBox(attributeHost_);
            combo->addItems(attribute.values);
            const int index = combo->findText(value);
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
            editor = combo;
            connect(combo, &QComboBox::currentTextChanged, this, [this]() {
                if (!updating_) {
                    emitAttributes();
                }
            });
        } else if (attribute.inputType == QStringLiteral("checkbox")) {
            auto* check = new QCheckBox(attributeHost_);
            check->setChecked(value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                              value == QStringLiteral("1"));
            editor = check;
            connect(check, &QCheckBox::toggled, this, [this]() {
                if (!updating_) {
                    emitAttributes();
                }
            });
        } else if (attribute.inputType == QStringLiteral("number")) {
            auto* line = new QLineEdit(value, attributeHost_);
            editor = line;
            connect(line, &QLineEdit::editingFinished, this, [this]() {
                if (!updating_) {
                    emitAttributes();
                }
            });
        } else {
            auto* line = new QLineEdit(value, attributeHost_);
            editor = line;
            connect(line, &QLineEdit::editingFinished, this, [this]() {
                if (!updating_) {
                    emitAttributes();
                }
            });
        }

        if (editor) {
            editor->setEnabled(attribute.mutableValue);
            entry.widget = editor;
            editors_.push_back(entry);
            attributeLayout_->addRow(attribute.name, editor);
        }
    }
    updating_ = false;
}

void LabelPanel::emitAttributes()
{
    QVector<AttributeValue> attributes;
    for (const auto& editor : editors_) {
        AttributeValue value;
        value.specId = editor.specId;
        if (auto* combo = qobject_cast<QComboBox*>(editor.widget)) {
            value.value = combo->currentText();
        } else if (auto* check = qobject_cast<QCheckBox*>(editor.widget)) {
            value.value = check->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (auto* line = qobject_cast<QLineEdit*>(editor.widget)) {
            value.value = line->text();
        }
        attributes.push_back(value);
    }
    emit attributesChanged(attributes);
}

void LabelPanel::showObject(const QString& key, int labelId,
                            const QVector<AttributeValue>& attributes, bool occluded,
                            const QString& summary)
{
    objectKey_ = key;
    objectLabelId_ = labelId;
    objectTitle_->setText(summary);
    deleteButton_->setEnabled(!key.isEmpty());
    occludedBox_->setEnabled(!key.isEmpty());

    updating_ = true;
    occludedBox_->setChecked(occluded);
    updating_ = false;

    rebuildAttributes(attributes);
}

void LabelPanel::clearObject()
{
    objectKey_.clear();
    objectLabelId_ = 0;
    objectTitle_->setText(QStringLiteral("No selection"));
    deleteButton_->setEnabled(false);
    occludedBox_->setEnabled(false);
    updating_ = true;
    occludedBox_->setChecked(false);
    updating_ = false;
    rebuildAttributes({});
}

} // namespace cvat
