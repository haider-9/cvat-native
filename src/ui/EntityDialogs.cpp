#include "cvatnative/ui/EntityDialogs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace cvat {

namespace {
QString colorFor(int index)
{
    static const QStringList colors{
        QStringLiteral("#4c9aff"), QStringLiteral("#f2c94c"), QStringLiteral("#6fcf97"),
        QStringLiteral("#bb6bd9"), QStringLiteral("#eb5757"), QStringLiteral("#56ccf2"),
        QStringLiteral("#f2994a"), QStringLiteral("#27ae60"),
    };
    return colors[index % colors.size()];
}

QJsonArray labelsFromTable(const QTableWidget* table)
{
    QJsonArray array;
    for (int row = 0; row < table->rowCount(); ++row) {
        const QTableWidgetItem* item = table->item(row, 0);
        const QString name = item ? item->text().trimmed() : QString();
        if (name.isEmpty()) {
            continue;
        }
        QJsonObject label;
        label.insert(QStringLiteral("name"), name);
        label.insert(QStringLiteral("color"), colorFor(row));
        label.insert(QStringLiteral("type"), QStringLiteral("any"));
        label.insert(QStringLiteral("attributes"), QJsonArray());
        array.push_back(label);
    }
    return array;
}

QTableWidget* makeLabelsTable(QWidget* parent)
{
    auto* table = new QTableWidget(0, 1, parent);
    table->setHorizontalHeaderLabels({QStringLiteral("Label name")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    return table;
}
} // namespace

ProjectDialog::ProjectDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("New project"));
    resize(420, 420);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("Project name"));
    form->addRow(QStringLiteral("Name"), nameEdit_);
    layout->addLayout(form);

    auto* group = new QGroupBox(QStringLiteral("Labels"), this);
    auto* groupLayout = new QVBoxLayout(group);
    labelsTable_ = makeLabelsTable(group);
    groupLayout->addWidget(labelsTable_);

    auto* buttons = new QHBoxLayout;
    auto* addButton = new QPushButton(QStringLiteral("Add label"), group);
    auto* removeButton = new QPushButton(QStringLiteral("Remove"), group);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    groupLayout->addLayout(buttons);
    layout->addWidget(group, 1);

    connect(addButton, &QPushButton::clicked, this, [this]() {
        const int row = labelsTable_->rowCount();
        labelsTable_->insertRow(row);
        labelsTable_->setItem(row, 0, new QTableWidgetItem(
            QStringLiteral("label_%1").arg(row + 1)));
        labelsTable_->editItem(labelsTable_->item(row, 0));
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        const int row = labelsTable_->currentRow();
        if (row >= 0) {
            labelsTable_->removeRow(row);
        }
    });

    labelsTable_->insertRow(0);
    labelsTable_->setItem(0, 0, new QTableWidgetItem(QStringLiteral("object")));

    auto* dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                               this);
    layout->addWidget(dialogButtons);
    connect(dialogButtons, &QDialogButtonBox::accepted, this, [this]() {
        if (projectName().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Project"),
                                 QStringLiteral("Enter a project name."));
            return;
        }
        if (labels().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Project"),
                                 QStringLiteral("Add at least one label."));
            return;
        }
        accept();
    });
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ProjectDialog::projectName() const
{
    return nameEdit_->text().trimmed();
}

QJsonArray ProjectDialog::labels() const
{
    return labelsFromTable(labelsTable_);
}

TaskDialog::TaskDialog(const QVector<Project>& projects, int defaultProjectId, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("New task"));
    resize(520, 600);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("Task name"));
    form->addRow(QStringLiteral("Name"), nameEdit_);

    projectCombo_ = new QComboBox(this);
    projectCombo_->addItem(QStringLiteral("(no project)"), 0);
    for (const auto& project : projects) {
        projectCombo_->addItem(project.name, project.id);
        if (project.id == defaultProjectId) {
            projectCombo_->setCurrentIndex(projectCombo_->count() - 1);
        }
    }
    form->addRow(QStringLiteral("Project"), projectCombo_);

    segmentSpin_ = new QSpinBox(this);
    segmentSpin_->setRange(1, 100000);
    segmentSpin_->setValue(100);
    form->addRow(QStringLiteral("Segment size"), segmentSpin_);

    overlapSpin_ = new QSpinBox(this);
    overlapSpin_->setRange(0, 1000);
    form->addRow(QStringLiteral("Overlap"), overlapSpin_);

    qualitySpin_ = new QSpinBox(this);
    qualitySpin_->setRange(5, 100);
    qualitySpin_->setValue(70);
    qualitySpin_->setSuffix(QStringLiteral(" %"));
    form->addRow(QStringLiteral("Image quality"), qualitySpin_);

    subsetEdit_ = new QLineEdit(this);
    form->addRow(QStringLiteral("Subset"), subsetEdit_);
    layout->addLayout(form);

    auto* labelGroup = new QGroupBox(
        QStringLiteral("Labels (used when no project is selected)"), this);
    auto* labelLayout = new QVBoxLayout(labelGroup);
    labelsTable_ = makeLabelsTable(labelGroup);
    labelsTable_->insertRow(0);
    labelsTable_->setItem(0, 0, new QTableWidgetItem(QStringLiteral("object")));
    labelLayout->addWidget(labelsTable_);
    layout->addWidget(labelGroup);

    auto* fileGroup = new QGroupBox(QStringLiteral("Data (images, video or archive)"), this);
    auto* fileLayout = new QVBoxLayout(fileGroup);
    filesList_ = new QListWidget(fileGroup);
    fileLayout->addWidget(filesList_);
    auto* fileButtons = new QHBoxLayout;
    auto* addFiles = new QPushButton(QStringLiteral("Add files…"), fileGroup);
    auto* removeFile = new QPushButton(QStringLiteral("Remove"), fileGroup);
    fileButtons->addWidget(addFiles);
    fileButtons->addWidget(removeFile);
    fileButtons->addStretch();
    fileLayout->addLayout(fileButtons);
    layout->addWidget(fileGroup, 1);

    connect(addFiles, &QPushButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, QStringLiteral("Select task data"), QString(),
            QStringLiteral("Media (*.jpg *.jpeg *.png *.bmp *.tif *.tiff *.webp *.mp4 "
                           "*.avi *.mov *.mkv *.zip);;All files (*)"));
        for (const auto& file : files) {
            if (filesList_->findItems(file, Qt::MatchExactly).isEmpty()) {
                filesList_->addItem(file);
            }
        }
    });
    connect(removeFile, &QPushButton::clicked, this, [this]() {
        delete filesList_->currentItem();
    });
    connect(projectCombo_, &QComboBox::currentIndexChanged, this,
            [this]() { updateLabelState(); });

    auto* dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                               this);
    layout->addWidget(dialogButtons);
    connect(dialogButtons, &QDialogButtonBox::accepted, this, [this]() {
        if (taskName().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Task"),
                                 QStringLiteral("Enter a task name."));
            return;
        }
        if (projectId() == 0 && labels().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Task"),
                                 QStringLiteral("Add at least one label for a standalone task."));
            return;
        }
        accept();
    });
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateLabelState();
}

void TaskDialog::updateLabelState()
{
    const bool standalone = projectId() == 0;
    labelsTable_->setEnabled(standalone);
}

QString TaskDialog::taskName() const { return nameEdit_->text().trimmed(); }
int TaskDialog::projectId() const { return projectCombo_->currentData().toInt(); }
int TaskDialog::segmentSize() const { return segmentSpin_->value(); }
int TaskDialog::overlap() const { return overlapSpin_->value(); }
int TaskDialog::imageQuality() const { return qualitySpin_->value(); }
QString TaskDialog::subset() const { return subsetEdit_->text().trimmed(); }
QJsonArray TaskDialog::labels() const { return labelsFromTable(labelsTable_); }

QStringList TaskDialog::files() const
{
    QStringList files;
    for (int row = 0; row < filesList_->count(); ++row) {
        files << filesList_->item(row)->text();
    }
    return files;
}

} // namespace cvat
