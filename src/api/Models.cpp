#include "cvatnative/api/Models.h"

#include <QJsonArray>

namespace cvat {

int intValue(const QJsonObject& json, const char* key, int defaultValue)
{
    const auto value = json.value(QLatin1String(key));
    if (value.isDouble()) {
        return value.toInt(defaultValue);
    }
    if (value.isString()) {
        bool ok = false;
        const int result = value.toString().toInt(&ok);
        return ok ? result : defaultValue;
    }
    return defaultValue;
}

QString stringValue(const QJsonObject& json, const char* key, const QString& defaultValue)
{
    const auto value = json.value(QLatin1String(key));
    return value.isString() ? value.toString() : defaultValue;
}

bool boolValue(const QJsonObject& json, const char* key, bool defaultValue)
{
    const auto value = json.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : defaultValue;
}

PageMeta pageMetaFromJson(const QJsonObject& json)
{
    PageMeta meta;
    meta.count = intValue(json, "count");
    const auto next = json.value(QStringLiteral("next"));
    const auto previous = json.value(QStringLiteral("previous"));
    if (next.isString()) {
        meta.next = next.toString();
    }
    if (previous.isString()) {
        meta.previous = previous.toString();
    }
    return meta;
}

QString UserInfo::displayName() const
{
    const QString full = (firstName + QLatin1Char(' ') + lastName).trimmed();
    return full.isEmpty() ? username : full;
}

UserInfo UserInfo::fromJson(const QJsonObject& json)
{
    UserInfo user;
    user.id = intValue(json, "id");
    user.username = stringValue(json, "username");
    user.firstName = stringValue(json, "first_name");
    user.lastName = stringValue(json, "last_name");
    user.email = stringValue(json, "email");
    user.isStaff = boolValue(json, "is_staff");
    user.isSuperuser = boolValue(json, "is_superuser");
    user.isActive = boolValue(json, "is_active");
    user.raw = json;
    return user;
}

Project Project::fromJson(const QJsonObject& json)
{
    Project project;
    project.id = intValue(json, "id");
    project.name = stringValue(json, "name");
    project.bugTracker = stringValue(json, "bug_tracker");
    project.status = stringValue(json, "status");
    project.dimension = stringValue(json, "dimension");
    project.createdDate = QDateTime::fromString(stringValue(json, "created_date"), Qt::ISODate);
    project.updatedDate = QDateTime::fromString(stringValue(json, "updated_date"), Qt::ISODate);
    project.owner = json.value(QStringLiteral("owner")).toObject();
    project.assignee = json.value(QStringLiteral("assignee")).toObject();
    const QJsonObject tasks = json.value(QStringLiteral("tasks")).toObject();
    if (!tasks.isEmpty()) {
        project.taskCount = intValue(tasks, "count");
    }
    for (const auto& subset : json.value(QStringLiteral("task_subsets")).toArray()) {
        project.taskSubsets.push_back(subset.toString());
    }
    project.raw = json;
    return project;
}

Organization Organization::fromJson(const QJsonObject& json)
{
    Organization organization;
    organization.id = intValue(json, "id");
    organization.slug = stringValue(json, "slug");
    organization.name = stringValue(json, "name");
    return organization;
}

Task Task::fromJson(const QJsonObject& json)
{
    Task task;
    task.id = intValue(json, "id");
    task.projectId = intValue(json, "project_id");
    task.dataId = intValue(json, "data");
    task.name = stringValue(json, "name");
    task.projectName = stringValue(json, "project_name");
    task.status = stringValue(json, "status");
    task.subset = stringValue(json, "subset");
    task.dimension = stringValue(json, "dimension");
    task.mode = stringValue(json, "mode");
    task.mediaType = stringValue(json, "media_type");
    task.size = intValue(json, "size");
    task.overlap = intValue(json, "overlap");
    task.segmentSize = intValue(json, "segment_size");
    task.imageQuality = intValue(json, "image_quality");
    task.createdDate = QDateTime::fromString(stringValue(json, "created_date"), Qt::ISODate);
    task.updatedDate = QDateTime::fromString(stringValue(json, "updated_date"), Qt::ISODate);
    task.owner = json.value(QStringLiteral("owner")).toObject();
    task.assignee = json.value(QStringLiteral("assignee")).toObject();
    task.raw = json;
    return task;
}

Job Job::fromJson(const QJsonObject& json)
{
    Job job;
    job.id = intValue(json, "id");
    job.taskId = intValue(json, "task_id");
    job.projectId = intValue(json, "project_id");
    job.taskName = stringValue(json, "task_name");
    job.projectName = stringValue(json, "project_name");
    job.status = stringValue(json, "status");
    job.stage = stringValue(json, "stage");
    job.state = stringValue(json, "state");
    job.type = stringValue(json, "type");
    job.dimension = stringValue(json, "dimension");
    job.mode = stringValue(json, "mode");
    job.mediaType = stringValue(json, "media_type");
    job.startFrame = intValue(json, "start_frame");
    job.stopFrame = intValue(json, "stop_frame");
    job.frameCount = intValue(json, "frame_count");
    job.dataChunkSize = intValue(json, "data_chunk_size");
    job.createdDate = QDateTime::fromString(stringValue(json, "created_date"), Qt::ISODate);
    job.updatedDate = QDateTime::fromString(stringValue(json, "updated_date"), Qt::ISODate);
    job.assignee = json.value(QStringLiteral("assignee")).toObject();
    job.raw = json;
    return job;
}

LabelAttribute LabelAttribute::fromJson(const QJsonObject& json)
{
    LabelAttribute attribute;
    attribute.id = intValue(json, "id");
    attribute.name = stringValue(json, "name");
    attribute.inputType = stringValue(json, "input_type", QStringLiteral("text"));
    attribute.mutableValue = boolValue(json, "mutable", true);
    attribute.defaultValue = stringValue(json, "default_value");
    for (const auto& value : json.value(QStringLiteral("values")).toArray()) {
        attribute.values.push_back(value.toString());
    }
    attribute.raw = json;
    return attribute;
}

Label Label::fromJson(const QJsonObject& json)
{
    Label label;
    label.id = intValue(json, "id");
    label.name = stringValue(json, "name");
    label.color = stringValue(json, "color", QStringLiteral("#b3b3b3"));
    label.type = stringValue(json, "type", QStringLiteral("any"));
    label.hasParent = boolValue(json, "has_parent");
    for (const auto& attribute : json.value(QStringLiteral("attributes")).toArray()) {
        label.attributes.push_back(LabelAttribute::fromJson(attribute.toObject()));
    }
    for (const auto& sublabel : json.value(QStringLiteral("sublabels")).toArray()) {
        label.sublabels.push_back(Label::fromJson(sublabel.toObject()));
    }
    label.raw = json;
    return label;
}

QJsonObject Label::toRequestJson(bool includeId) const
{
    QJsonObject json;
    if (includeId && id > 0) {
        json.insert(QStringLiteral("id"), id);
    }
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("color"), color);
    json.insert(QStringLiteral("type"), type);

    QJsonArray attributeArray;
    for (const auto& attribute : attributes) {
        QJsonObject attributeJson;
        if (attribute.id > 0) {
            attributeJson.insert(QStringLiteral("id"), attribute.id);
        }
        attributeJson.insert(QStringLiteral("name"), attribute.name);
        attributeJson.insert(QStringLiteral("input_type"), attribute.inputType);
        attributeJson.insert(QStringLiteral("mutable"), attribute.mutableValue);
        if (!attribute.defaultValue.isEmpty()) {
            attributeJson.insert(QStringLiteral("default_value"), attribute.defaultValue);
        }
        QJsonArray values;
        for (const auto& value : attribute.values) {
            values.push_back(value);
        }
        attributeJson.insert(QStringLiteral("values"), values);
        attributeArray.push_back(attributeJson);
    }
    json.insert(QStringLiteral("attributes"), attributeArray);

    if (!sublabels.isEmpty()) {
        QJsonArray sublabelArray;
        for (const auto& sublabel : sublabels) {
            sublabelArray.push_back(sublabel.toRequestJson(includeId));
        }
        json.insert(QStringLiteral("sublabels"), sublabelArray);
    }
    return json;
}

FrameInfo FrameInfo::fromJson(const QJsonObject& json)
{
    FrameInfo frame;
    frame.width = intValue(json, "width");
    frame.height = intValue(json, "height");
    frame.name = stringValue(json, "name");
    frame.relatedFiles = intValue(json, "related_files");
    return frame;
}

DataMeta DataMeta::fromJson(const QJsonObject& json)
{
    DataMeta meta;
    meta.size = intValue(json, "size");
    meta.startFrame = intValue(json, "start_frame");
    meta.stopFrame = intValue(json, "stop_frame");
    meta.chunkSize = intValue(json, "chunk_size");
    meta.imageQuality = intValue(json, "image_quality");
    meta.frameFilter = stringValue(json, "frame_filter");
    meta.chunksUpdatedDate = QDateTime::fromString(stringValue(json, "chunks_updated_date"),
                                                   Qt::ISODate);
    for (const auto& frame : json.value(QStringLiteral("frames")).toArray()) {
        meta.frames.push_back(FrameInfo::fromJson(frame.toObject()));
    }
    for (const auto& frame : json.value(QStringLiteral("deleted_frames")).toArray()) {
        meta.deletedFrames.push_back(frame.toInt());
    }
    for (const auto& frame : json.value(QStringLiteral("included_frames")).toArray()) {
        meta.includedFrames.push_back(frame.toInt());
    }
    meta.raw = json;
    return meta;
}

bool DataMeta::isDeleted(int frame) const
{
    return deletedFrames.contains(frame);
}

RequestState RequestState::fromJson(const QJsonObject& json)
{
    RequestState state;
    state.id = stringValue(json, "id");
    state.status = stringValue(json, "status");
    state.progress = json.value(QStringLiteral("progress")).toDouble();
    state.message = stringValue(json, "message");
    state.resultUrl = stringValue(json, "result_url");
    state.resultId = intValue(json, "result_id");
    state.operation = json.value(QStringLiteral("operation")).toObject();
    state.raw = json;
    return state;
}

ServerInfo ServerInfo::fromJson(const QJsonObject& json)
{
    ServerInfo info;
    info.name = stringValue(json, "name");
    info.description = stringValue(json, "description");
    info.version = stringValue(json, "version");
    info.logoUrl = stringValue(json, "logo_url");
    info.raw = json;
    return info;
}

FormatInfo FormatInfo::fromJson(const QJsonObject& json)
{
    FormatInfo info;
    info.name = stringValue(json, "name");
    info.extension = stringValue(json, "ext");
    info.version = stringValue(json, "version");
    info.enabled = boolValue(json, "enabled");
    info.dimension = stringValue(json, "dimension");
    return info;
}

Formats Formats::fromJson(const QJsonObject& json)
{
    Formats formats;
    for (const auto& item : json.value(QStringLiteral("importers")).toArray()) {
        formats.importers.push_back(FormatInfo::fromJson(item.toObject()));
    }
    for (const auto& item : json.value(QStringLiteral("exporters")).toArray()) {
        formats.exporters.push_back(FormatInfo::fromJson(item.toObject()));
    }
    return formats;
}

} // namespace cvat
