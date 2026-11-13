#pragma once

#include "cvatnative/api/Models.h"
#include "cvatnative/model/Annotation.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QUndoStack>
#include <QVector>

#include <optional>

namespace cvat {

struct ShapeDelta {
    QString key;
    std::optional<Shape> before;
    std::optional<Shape> after;
    std::optional<Track> beforeTrack;
    std::optional<Track> afterTrack;
    std::optional<Tag> beforeTag;
    std::optional<Tag> afterTag;
    int beforeIndex = -1;
    int afterIndex = -1;
};

class ShapeChangeCommand;

// Editable annotation document for one job. The model owns the CVAT
// LabeledData, selection, undo/redo and the diff against the last synchronized
// server state. It deliberately contains no HTTP or UI code.
class AnnotationModel : public QObject {
    Q_OBJECT
public:
    struct VisibleShape {
        QString key;   // "s:<id>" for a static shape, "t:<id>" for a track
        int objectId = 0;
        bool fromTrack = false;
        Shape shape;   // interpolated for tracks, at the current frame
    };

    struct PatchSet {
        LabeledData create;
        LabeledData update;
        LabeledData remove;

        [[nodiscard]] bool any() const;
        [[nodiscard]] bool hasCreate() const;
        [[nodiscard]] bool hasUpdate() const;
        [[nodiscard]] bool hasRemove() const;
    };

    explicit AnnotationModel(QObject* parent = nullptr);

    void setLabels(const QVector<Label>& labels);
    [[nodiscard]] const QVector<Label>& labels() const { return labels_; }
    [[nodiscard]] const Label* label(int id) const;

    void load(const LabeledData& data, int frame);
    void reset();

    [[nodiscard]] const LabeledData& data() const { return data_; }
    [[nodiscard]] const LabeledData& syncedData() const { return synced_; }
    [[nodiscard]] quint64 revision() const { return revision_; }

    [[nodiscard]] int currentFrame() const { return currentFrame_; }
    void setCurrentFrame(int frame);

    [[nodiscard]] QVector<VisibleShape> visibleShapes() const;
    [[nodiscard]] QVector<Tag> visibleTags() const;
    [[nodiscard]] std::optional<VisibleShape> visibleShape(const QString& key) const;

    void addShape(Shape shape);
    // Adds several shapes as one undoable step and selects them all.
    void addShapes(const QVector<Shape>& shapes);
    bool updateVisibleShape(const QString& key, const Shape& shape);
    int updateVisibleShapes(const QVector<QPair<QString, Shape>>& updates);
    bool removeObject(const QString& key);
    bool removeObjects(const QSet<QString>& keys);

    bool setObjectLabel(const QString& key, int labelId);
    bool setObjectAttributes(const QString& key, const QVector<AttributeValue>& attributes);
    bool setObjectOccluded(const QString& key, bool occluded);

    [[nodiscard]] bool isDirty() const { return dirty_; }
    void markClean();

    [[nodiscard]] PatchSet buildPatch() const;
    [[nodiscard]] bool hasChanges() const;

    [[nodiscard]] bool canUndo() const { return undoStack_.canUndo(); }
    [[nodiscard]] bool canRedo() const { return undoStack_.canRedo(); }
    bool undo();
    bool redo();

    [[nodiscard]] const QSet<QString>& selection() const { return selection_; }
    void setSelection(const QSet<QString>& keys);
    void selectOnly(const QString& key);
    void toggleSelection(const QString& key);
    void clearSelection();

    [[nodiscard]] int nextZOrder() const;
    [[nodiscard]] QString keyForShape(const Shape& shape) const;
    [[nodiscard]] QString keyForTrack(const Track& track) const;
    [[nodiscard]] QString keyForTag(const Tag& tag) const;

signals:
    void changed();
    void selectionChanged();
    void frameChanged(int frame);
    void dirtyChanged(bool dirty);
    void undoStackChanged();

private:
    friend class ShapeChangeCommand;

    void beginMutation();
    void endMutation();
    void applyDeltas(const QVector<ShapeDelta>& deltas, bool reverse);
    void setDirty(bool dirty);
    Shape* shapeById(int id);
    const Shape* shapeById(int id) const;
    Track* trackById(int id);
    const Track* trackById(int id) const;
    Tag* tagById(int id);
    const Tag* tagById(int id) const;

    LabeledData data_;
    LabeledData synced_;
    QVector<Label> labels_;
    QUndoStack undoStack_;
    LabeledData mutationBefore_;
    QSet<QString> selection_;
    int currentFrame_ = 0;
    int nextLocalId_ = -1;
    quint64 revision_ = 0;
    bool dirty_ = false;
};

} // namespace cvat
