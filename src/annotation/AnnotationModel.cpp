#include "cvatnative/annotation/AnnotationModel.h"

#include "cvatnative/annotation/Geometry.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace cvat {

namespace {

constexpr int kUndoLimit = 80;

void assignLocalIds(Shape& shape, int& nextId)
{
    if (!shape.id.has_value()) {
        shape.id = nextId--;
    }
    for (auto& element : shape.elements) {
        assignLocalIds(element, nextId);
    }
}

void assignLocalIds(Track& track, int& nextId)
{
    if (!track.id.has_value()) {
        track.id = nextId--;
    }
    for (auto& shape : track.shapes) {
        if (!shape.id.has_value()) {
            shape.id = nextId--;
        }
    }
    for (auto& element : track.elements) {
        assignLocalIds(element, nextId);
    }
}

void assignLocalIds(Tag& tag, int& nextId)
{
    if (!tag.id.has_value()) {
        tag.id = nextId--;
    }
}

QJsonObject stripped(Shape shape)
{
    shape.id.reset();
    QJsonArray elements;
    for (auto element : shape.elements) {
        elements.push_back(stripped(element));
    }
    shape.elements.clear();
    QJsonObject json = shape.toJson();
    json.remove(QStringLiteral("id"));
    if (!elements.isEmpty()) {
        json.insert(QStringLiteral("elements"), elements);
    }
    return json;
}

QJsonObject stripped(TrackedShape shape)
{
    shape.id.reset();
    QJsonObject json = shape.toJson();
    json.remove(QStringLiteral("id"));
    return json;
}

QJsonObject stripped(Track track)
{
    track.id.reset();
    QJsonArray shapes;
    for (auto shape : track.shapes) {
        shapes.push_back(stripped(shape));
    }
    QJsonArray elements;
    for (auto element : track.elements) {
        elements.push_back(stripped(element));
    }
    track.shapes.clear();
    track.elements.clear();
    QJsonObject json = track.toJson();
    json.remove(QStringLiteral("id"));
    json.insert(QStringLiteral("shapes"), shapes);
    if (!elements.isEmpty()) {
        json.insert(QStringLiteral("elements"), elements);
    } else {
        json.remove(QStringLiteral("elements"));
    }
    return json;
}

QJsonObject stripped(Tag tag)
{
    tag.id.reset();
    return tag.toJson();
}

Shape shapeFromTracked(const TrackedShape& tracked)
{
    Shape shape;
    shape.id = tracked.id;
    shape.type = tracked.type;
    shape.points = tracked.points;
    shape.frame = tracked.frame;
    shape.occluded = tracked.occluded;
    shape.outside = tracked.outside;
    shape.zOrder = tracked.zOrder;
    shape.rotation = tracked.rotation;
    shape.attributes = tracked.attributes;
    return shape;
}

bool differs(const Shape& a, const Shape& b) { return !(a == b); }
bool differs(const Track& a, const Track& b) { return !(a == b); }
bool differs(const Tag& a, const Tag& b) { return !(a == b); }

} // namespace

AnnotationModel::AnnotationModel(QObject* parent) : QObject(parent) {}

void AnnotationModel::setLabels(const QVector<Label>& labels)
{
    labels_ = labels;
    emit changed();
}

const Label* AnnotationModel::label(int id) const
{
    for (const auto& label : labels_) {
        if (label.id == id) {
            return &label;
        }
    }
    return nullptr;
}

void AnnotationModel::load(const LabeledData& data, int frame)
{
    data_ = data;
    for (auto& shape : data_.shapes) {
        assignLocalIds(shape, nextLocalId_);
    }
    for (auto& track : data_.tracks) {
        assignLocalIds(track, nextLocalId_);
    }
    for (auto& tag : data_.tags) {
        assignLocalIds(tag, nextLocalId_);
    }
    synced_ = data_;
    undo_.clear();
    redo_.clear();
    selection_.clear();
    currentFrame_ = std::max(0, frame);
    setDirty(false);
    emit changed();
    emit selectionChanged();
    emit frameChanged(currentFrame_);
    emit undoStackChanged();
}

void AnnotationModel::reset()
{
    data_ = {};
    synced_ = {};
    undo_.clear();
    redo_.clear();
    selection_.clear();
    nextLocalId_ = -1;
    currentFrame_ = 0;
    setDirty(false);
    emit changed();
    emit selectionChanged();
    emit frameChanged(0);
    emit undoStackChanged();
}

void AnnotationModel::setCurrentFrame(int frame)
{
    frame = std::max(0, frame);
    if (frame == currentFrame_) {
        return;
    }
    currentFrame_ = frame;
    selection_.clear();
    emit frameChanged(frame);
    emit selectionChanged();
    emit changed();
}

void AnnotationModel::beginMutation()
{
    undo_.push_back(data_);
    if (undo_.size() > kUndoLimit) {
        undo_.removeFirst();
    }
    redo_.clear();
}

void AnnotationModel::endMutation()
{
    setDirty(true);
    emit changed();
    emit undoStackChanged();
}

void AnnotationModel::setDirty(bool dirty)
{
    if (dirty_ == dirty) {
        return;
    }
    dirty_ = dirty;
    emit dirtyChanged(dirty_);
}

QString AnnotationModel::keyForShape(const Shape& shape) const
{
    return QStringLiteral("s:%1").arg(shape.id.value_or(0));
}

QString AnnotationModel::keyForTrack(const Track& track) const
{
    return QStringLiteral("t:%1").arg(track.id.value_or(0));
}

QString AnnotationModel::keyForTag(const Tag& tag) const
{
    return QStringLiteral("g:%1").arg(tag.id.value_or(0));
}

QVector<AnnotationModel::VisibleShape> AnnotationModel::visibleShapes() const
{
    QVector<VisibleShape> visible;
    for (const auto& shape : data_.shapes) {
        if (shape.frame != currentFrame_ || shape.outside) {
            continue;
        }
        VisibleShape item;
        item.key = keyForShape(shape);
        item.objectId = shape.id.value_or(0);
        item.fromTrack = false;
        item.shape = shape;
        visible.push_back(item);
    }
    for (const auto& track : data_.tracks) {
        const auto interpolated = track.interpolatedAt(currentFrame_);
        if (!interpolated || interpolated->outside) {
            continue;
        }
        VisibleShape item;
        item.key = keyForTrack(track);
        item.objectId = track.id.value_or(0);
        item.fromTrack = true;
        item.shape = shapeFromTracked(*interpolated);
        item.shape.labelId = track.labelId;
        item.shape.frame = currentFrame_;
        item.shape.group = track.group;
        visible.push_back(item);
    }
    std::sort(visible.begin(), visible.end(), [](const VisibleShape& a, const VisibleShape& b) {
        if (a.shape.zOrder != b.shape.zOrder) {
            return a.shape.zOrder < b.shape.zOrder;
        }
        return a.objectId < b.objectId;
    });
    return visible;
}

QVector<Tag> AnnotationModel::visibleTags() const
{
    QVector<Tag> tags;
    for (const auto& tag : data_.tags) {
        if (tag.frame == currentFrame_) {
            tags.push_back(tag);
        }
    }
    return tags;
}

std::optional<AnnotationModel::VisibleShape> AnnotationModel::visibleShape(
    const QString& key) const
{
    for (auto& shape : visibleShapes()) {
        if (shape.key == key) {
            return shape;
        }
    }
    return std::nullopt;
}

int AnnotationModel::nextZOrder() const
{
    int maximum = 0;
    for (const auto& shape : data_.shapes) {
        maximum = std::max(maximum, shape.zOrder);
    }
    for (const auto& track : data_.tracks) {
        for (const auto& shape : track.shapes) {
            maximum = std::max(maximum, shape.zOrder);
        }
    }
    return maximum + 1;
}

void AnnotationModel::addShape(Shape shape)
{
    beginMutation();
    shape.id = nextLocalId_--;
    shape.frame = currentFrame_;
    shape.zOrder = nextZOrder();
    data_.shapes.push_back(shape);
    selection_ = {keyForShape(shape)};
    endMutation();
    emit selectionChanged();
}

bool AnnotationModel::updateVisibleShape(const QString& key, const Shape& shape)
{
    if (!key.contains(QLatin1Char(':'))) {
        return false;
    }
    const QChar kind = key.at(0);
    const int id = key.mid(2).toInt();

    if (kind == QLatin1Char('s')) {
        Shape* target = shapeById(id);
        if (!target) {
            return false;
        }
        beginMutation();
        const std::optional<int> persistedId = target->id;
        *target = shape;
        target->id = persistedId;
        target->frame = currentFrame_;
        endMutation();
        return true;
    }
    if (kind == QLatin1Char('t')) {
        Track* track = trackById(id);
        if (!track) {
            return false;
        }
        beginMutation();
        TrackedShape replacement;
        replacement.type = shape.type;
        replacement.points = shape.points;
        replacement.frame = currentFrame_;
        replacement.occluded = shape.occluded;
        replacement.outside = shape.outside;
        replacement.zOrder = shape.zOrder;
        replacement.rotation = shape.rotation;
        replacement.attributes = shape.attributes;

        auto it = std::find_if(track->shapes.begin(), track->shapes.end(),
                               [this](const TrackedShape& keyframe) {
            return keyframe.frame == currentFrame_;
        });
        if (it != track->shapes.end()) {
            replacement.id = it->id;
            *it = replacement;
        } else {
            track->shapes.push_back(replacement);
            std::sort(track->shapes.begin(), track->shapes.end(),
                      [](const TrackedShape& a, const TrackedShape& b) {
                return a.frame < b.frame;
            });
        }
        endMutation();
        return true;
    }
    return false;
}

bool AnnotationModel::removeObject(const QString& key)
{
    return removeObjects({key});
}

bool AnnotationModel::removeObjects(const QSet<QString>& keys)
{
    if (keys.isEmpty()) {
        return false;
    }
    bool removed = false;
    beginMutation();
    for (const QString& key : keys) {
        if (key.size() < 3) {
            continue;
        }
        const QChar kind = key.at(0);
        const int id = key.mid(2).toInt();
        if (kind == QLatin1Char('s')) {
            const auto oldSize = data_.shapes.size();
            data_.shapes.erase(std::remove_if(data_.shapes.begin(), data_.shapes.end(),
                                              [id](const Shape& shape) {
                return shape.id.value_or(0) == id;
            }), data_.shapes.end());
            removed = removed || data_.shapes.size() != oldSize;
        } else if (kind == QLatin1Char('t')) {
            const auto oldSize = data_.tracks.size();
            data_.tracks.erase(std::remove_if(data_.tracks.begin(), data_.tracks.end(),
                                              [id](const Track& track) {
                return track.id.value_or(0) == id;
            }), data_.tracks.end());
            removed = removed || data_.tracks.size() != oldSize;
        } else if (kind == QLatin1Char('g')) {
            const auto oldSize = data_.tags.size();
            data_.tags.erase(std::remove_if(data_.tags.begin(), data_.tags.end(),
                                            [id](const Tag& tag) {
                return tag.id.value_or(0) == id;
            }), data_.tags.end());
            removed = removed || data_.tags.size() != oldSize;
        }
    }
    if (!removed) {
        undo_.removeLast();
        return false;
    }
    for (const auto& key : keys) {
        selection_.remove(key);
    }
    endMutation();
    emit selectionChanged();
    return true;
}

bool AnnotationModel::setObjectLabel(const QString& key, int labelId)
{
    if (key.size() < 3) {
        return false;
    }
    const QChar kind = key.at(0);
    const int id = key.mid(2).toInt();
    beginMutation();
    bool changedValue = false;
    if (kind == QLatin1Char('s')) {
        if (Shape* shape = shapeById(id)) {
            changedValue = shape->labelId != labelId;
            shape->labelId = labelId;
        }
    } else if (kind == QLatin1Char('t')) {
        if (Track* track = trackById(id)) {
            changedValue = track->labelId != labelId;
            track->labelId = labelId;
        }
    } else if (kind == QLatin1Char('g')) {
        if (Tag* tag = tagById(id)) {
            changedValue = tag->labelId != labelId;
            tag->labelId = labelId;
        }
    }
    if (!changedValue) {
        undo_.removeLast();
        return false;
    }
    endMutation();
    return true;
}

bool AnnotationModel::setObjectAttributes(const QString& key,
                                          const QVector<AttributeValue>& attributes)
{
    if (key.size() < 3) {
        return false;
    }
    const QChar kind = key.at(0);
    const int id = key.mid(2).toInt();
    beginMutation();
    bool changedValue = false;
    if (kind == QLatin1Char('s')) {
        if (Shape* shape = shapeById(id)) {
            changedValue = shape->attributes != attributes;
            shape->attributes = attributes;
        }
    } else if (kind == QLatin1Char('t')) {
        if (Track* track = trackById(id)) {
            changedValue = track->attributes != attributes;
            track->attributes = attributes;
            for (auto& keyframe : track->shapes) {
                if (keyframe.frame == currentFrame_) {
                    keyframe.attributes = attributes;
                    break;
                }
            }
        }
    } else if (kind == QLatin1Char('g')) {
        if (Tag* tag = tagById(id)) {
            changedValue = tag->attributes != attributes;
            tag->attributes = attributes;
        }
    }
    if (!changedValue) {
        undo_.removeLast();
        return false;
    }
    endMutation();
    return true;
}

bool AnnotationModel::setObjectOccluded(const QString& key, bool occluded)
{
    if (key.size() < 3) {
        return false;
    }
    const QChar kind = key.at(0);
    const int id = key.mid(2).toInt();
    beginMutation();
    bool changedValue = false;
    if (kind == QLatin1Char('s')) {
        if (Shape* shape = shapeById(id)) {
            changedValue = shape->occluded != occluded;
            shape->occluded = occluded;
        }
    } else if (kind == QLatin1Char('t')) {
        if (Track* track = trackById(id)) {
            for (auto& keyframe : track->shapes) {
                if (keyframe.frame == currentFrame_) {
                    changedValue = keyframe.occluded != occluded;
                    keyframe.occluded = occluded;
                    break;
                }
            }
        }
    }
    if (!changedValue) {
        undo_.removeLast();
        return false;
    }
    endMutation();
    return true;
}

void AnnotationModel::markClean()
{
    synced_ = data_;
    setDirty(false);
}

bool AnnotationModel::hasChanges() const
{
    return data_.toJson() != synced_.toJson();
}

AnnotationModel::PatchSet AnnotationModel::buildPatch() const
{
    PatchSet patch;
    patch.create.version = data_.version;
    patch.update.version = data_.version;
    patch.remove.version = data_.version;

    auto appendShape = [&patch](LabeledData& target, const Shape& shape, bool create) {
        if (create) {
            target.shapes.push_back(Shape::fromJson(stripped(shape)));
        } else {
            target.shapes.push_back(shape);
        }
    };

    for (const auto& current : data_.shapes) {
        const int id = current.id.value_or(-1);
        if (id < 0) {
            appendShape(patch.create, current, true);
            continue;
        }
        const auto found = std::find_if(synced_.shapes.begin(), synced_.shapes.end(),
                                        [id](const Shape& shape) {
            return shape.id.value_or(-1) == id;
        });
        if (found == synced_.shapes.end()) {
            patch.create.shapes.push_back(Shape::fromJson(stripped(current)));
        } else if (differs(current, *found)) {
            patch.update.shapes.push_back(current);
        }
    }
    for (const auto& previous : synced_.shapes) {
        const int id = previous.id.value_or(-1);
        if (id < 0) {
            continue;
        }
        const bool stillPresent = std::any_of(data_.shapes.begin(), data_.shapes.end(),
                                              [id](const Shape& shape) {
            return shape.id.value_or(-1) == id;
        });
        if (!stillPresent) {
            patch.remove.shapes.push_back(previous);
        }
    }

    for (const auto& current : data_.tracks) {
        const int id = current.id.value_or(-1);
        if (id < 0) {
            patch.create.tracks.push_back(Track::fromJson(stripped(current)));
            continue;
        }
        const auto found = std::find_if(synced_.tracks.begin(), synced_.tracks.end(),
                                        [id](const Track& track) {
            return track.id.value_or(-1) == id;
        });
        if (found == synced_.tracks.end()) {
            patch.create.tracks.push_back(Track::fromJson(stripped(current)));
        } else if (differs(current, *found)) {
            patch.update.tracks.push_back(current);
        }
    }
    for (const auto& previous : synced_.tracks) {
        const int id = previous.id.value_or(-1);
        if (id < 0) {
            continue;
        }
        const bool stillPresent = std::any_of(data_.tracks.begin(), data_.tracks.end(),
                                              [id](const Track& track) {
            return track.id.value_or(-1) == id;
        });
        if (!stillPresent) {
            patch.remove.tracks.push_back(previous);
        }
    }

    for (const auto& current : data_.tags) {
        const int id = current.id.value_or(-1);
        if (id < 0) {
            Tag copy = current;
            copy.id.reset();
            patch.create.tags.push_back(copy);
            continue;
        }
        const auto found = std::find_if(synced_.tags.begin(), synced_.tags.end(),
                                        [id](const Tag& tag) {
            return tag.id.value_or(-1) == id;
        });
        if (found == synced_.tags.end()) {
            patch.create.tags.push_back(Tag::fromJson(stripped(current)));
        } else if (differs(current, *found)) {
            patch.update.tags.push_back(current);
        }
    }
    for (const auto& previous : synced_.tags) {
        const int id = previous.id.value_or(-1);
        if (id < 0) {
            continue;
        }
        const bool stillPresent = std::any_of(data_.tags.begin(), data_.tags.end(),
                                              [id](const Tag& tag) {
            return tag.id.value_or(-1) == id;
        });
        if (!stillPresent) {
            patch.remove.tags.push_back(previous);
        }
    }

    return patch;
}

bool AnnotationModel::PatchSet::any() const
{
    return hasCreate() || hasUpdate() || hasRemove();
}

bool AnnotationModel::PatchSet::hasCreate() const
{
    return !create.shapes.isEmpty() || !create.tracks.isEmpty() || !create.tags.isEmpty();
}

bool AnnotationModel::PatchSet::hasUpdate() const
{
    return !update.shapes.isEmpty() || !update.tracks.isEmpty() || !update.tags.isEmpty();
}

bool AnnotationModel::PatchSet::hasRemove() const
{
    return !remove.shapes.isEmpty() || !remove.tracks.isEmpty() || !remove.tags.isEmpty();
}

bool AnnotationModel::undo()
{
    if (undo_.isEmpty()) {
        return false;
    }
    redo_.push_back(data_);
    data_ = undo_.takeLast();
    selection_.clear();
    setDirty(true);
    emit changed();
    emit selectionChanged();
    emit undoStackChanged();
    return true;
}

bool AnnotationModel::redo()
{
    if (redo_.isEmpty()) {
        return false;
    }
    undo_.push_back(data_);
    data_ = redo_.takeLast();
    selection_.clear();
    setDirty(true);
    emit changed();
    emit selectionChanged();
    emit undoStackChanged();
    return true;
}

void AnnotationModel::setSelection(const QSet<QString>& keys)
{
    if (selection_ == keys) {
        return;
    }
    selection_ = keys;
    emit selectionChanged();
}

void AnnotationModel::selectOnly(const QString& key)
{
    setSelection({key});
}

void AnnotationModel::toggleSelection(const QString& key)
{
    QSet<QString> next = selection_;
    if (next.contains(key)) {
        next.remove(key);
    } else {
        next.insert(key);
    }
    setSelection(next);
}

void AnnotationModel::clearSelection()
{
    setSelection({});
}

Shape* AnnotationModel::shapeById(int id)
{
    for (auto& shape : data_.shapes) {
        if (shape.id.value_or(0) == id) {
            return &shape;
        }
    }
    return nullptr;
}

const Shape* AnnotationModel::shapeById(int id) const
{
    for (const auto& shape : data_.shapes) {
        if (shape.id.value_or(0) == id) {
            return &shape;
        }
    }
    return nullptr;
}

Track* AnnotationModel::trackById(int id)
{
    for (auto& track : data_.tracks) {
        if (track.id.value_or(0) == id) {
            return &track;
        }
    }
    return nullptr;
}

const Track* AnnotationModel::trackById(int id) const
{
    for (const auto& track : data_.tracks) {
        if (track.id.value_or(0) == id) {
            return &track;
        }
    }
    return nullptr;
}

Tag* AnnotationModel::tagById(int id)
{
    for (auto& tag : data_.tags) {
        if (tag.id.value_or(0) == id) {
            return &tag;
        }
    }
    return nullptr;
}

const Tag* AnnotationModel::tagById(int id) const
{
    for (const auto& tag : data_.tags) {
        if (tag.id.value_or(0) == id) {
            return &tag;
        }
    }
    return nullptr;
}

} // namespace cvat
