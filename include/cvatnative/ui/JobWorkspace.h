#pragma once

#include "cvatnative/api/Models.h"
#include "cvatnative/model/Annotation.h"
#include "cvatnative/ui/AnnotationCanvas.h"

#include <QWidget>

#include <functional>

class QAction;
class QActionGroup;
class QLabel;
class QListWidget;
class QResizeEvent;
class QSlider;
class QSpinBox;
class QTimer;
class QToolBar;

namespace cvat {

class AnnotationsApi;
class AnnotationCanvas;
class AnnotationModel;
class JobsApi;
class LabelsApi;
class LabelPanel;
class LocalStore;
class MediaApi;
class MediaService;
class RecoveryStore;
class RequestHandle;
class ServerApi;
class ToastWidget;

class JobWorkspace : public QWidget {
    Q_OBJECT
public:
    JobWorkspace(const Job& job, AnnotationsApi* annotations, JobsApi* jobs,
                 LabelsApi* labels, MediaService* media,
                 RecoveryStore* recovery, LocalStore* store,
                 QWidget* parent = nullptr);
    ~JobWorkspace() override;

    void applySettings();

    // Asks the user about unsaved work before emitting backRequested().
    void requestBack();
    [[nodiscard]] bool isDirty() const;

signals:
    void backRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void connectModel();
    void load();
    void loadLabels();
    void loadMeta();
    void loadAnnotations();
    void loadFrame(int frame);
    void setFrame(int frame);
    void requestFrame(int frame);

    void refreshObjects();
    void updateSelectionUi();
    void updateActions();
    void updateTitle();
    void setStatus(const QString& message, bool error = false);
    void showToast(const QString& message, bool isError = false, int durationMs = 3000);
    void setTool(int tool);

    void saveRemote(std::function<void(bool)> done = {}, bool silent = false);
    void saveLocalSnapshot();
    void copyLabel();
    void pasteLabel();
    void copyAnnotations();
    void pasteAnnotations();
    void showGrowDialog();
    void showMoveDialog();
    void showRelabelDialog();
    void showDeleteDialog();
    void deleteSelection();
    void changeSelectionLabel(int labelId);
    void changeAttributes(const QVector<AttributeValue>& attributes);
    void changeOccluded(bool occluded);

    [[nodiscard]] QString selectedKey() const;
    [[nodiscard]] int firstFrame() const;
    [[nodiscard]] int lastFrame() const;

    Job job_;
    AnnotationsApi* annotations_;
    JobsApi* jobs_;
    LabelsApi* labelsApi_;
    MediaService* media_;
    RecoveryStore* recovery_;
    LocalStore* store_;

    AnnotationModel* model_ = nullptr;
    AnnotationCanvas* canvas_ = nullptr;
    LabelPanel* labelPanel_ = nullptr;
    QListWidget* objectList_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* dirtyLabel_ = nullptr;
    ToastWidget* toast_ = nullptr;
    QSpinBox* frameSpin_ = nullptr;
    QSlider* frameSlider_ = nullptr;
    QToolBar* toolbar_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* deleteAction_ = nullptr;
    QActionGroup* toolGroup_ = nullptr;
    QTimer* autosaveTimer_ = nullptr;
    QTimer* frameDebounce_ = nullptr;
    RequestHandle* frameRequest_ = nullptr;
    quint64 frameGeneration_ = 0;
    quint64 lastSavedRevision_ = 0;

    QVector<Label> labels_;
    CopiedTemplate copiedTemplate_;
    int copiedLabelId_ = 0;
    bool annotationsLoaded_ = false;
    bool saving_ = false;
    bool snapshotBusy_ = false;
    bool snapshotPending_ = false;
    bool recoveryChecked_ = false;
    int requestedFrame_ = -1;
    int frameCount_ = 1;
};

} // namespace cvat
