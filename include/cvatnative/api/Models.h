#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace cvat {

struct UserInfo {
    int id = 0;
    QString username;
    QString firstName;
    QString lastName;
    QString email;
    bool isStaff = false;
    bool isSuperuser = false;
    bool isActive = false;
    QJsonObject raw;

    [[nodiscard]] QString displayName() const;
    static UserInfo fromJson(const QJsonObject& json);
};

struct PageMeta {
    int count = 0;
    QString next;
    QString previous;

    [[nodiscard]] bool hasNext() const { return !next.isEmpty(); }
};

template <typename T>
struct Page {
    PageMeta meta;
    QVector<T> results;
};

PageMeta pageMetaFromJson(const QJsonObject& json);

struct Project {
    int id = 0;
    QString name;
    QString bugTracker;
    QString status;
    QString dimension;
    QDateTime createdDate;
    QDateTime updatedDate;
    int taskCount = 0;
    QJsonObject owner;
    QJsonObject assignee;
    QVector<QString> taskSubsets;
    QJsonObject raw;

    static Project fromJson(const QJsonObject& json);
};

struct Organization {
    int id = 0;
    QString slug;
    QString name;

    static Organization fromJson(const QJsonObject& json);
};

struct Task {
    int id = 0;
    int projectId = 0;
    int dataId = 0;
    QString name;
    QString projectName;
    QString status;
    QString subset;
    QString dimension;
    QString mode;
    QString mediaType;
    int size = 0;
    int overlap = 0;
    int segmentSize = 0;
    int imageQuality = 0;
    int validationMode = 0;
    QDateTime createdDate;
    QDateTime updatedDate;
    QJsonObject owner;
    QJsonObject assignee;
    QJsonObject raw;

    static Task fromJson(const QJsonObject& json);
};

struct Job {
    int id = 0;
    int taskId = 0;
    int projectId = 0;
    QString taskName;
    QString projectName;
    QString status;
    QString stage;
    QString state;
    QString type;
    QString dimension;
    QString mode;
    QString mediaType;
    int startFrame = 0;
    int stopFrame = 0;
    int frameCount = 0;
    int dataChunkSize = 0;
    QDateTime createdDate;
    QDateTime updatedDate;
    QJsonObject assignee;
    QJsonObject raw;

    static Job fromJson(const QJsonObject& json);
};

struct LabelAttribute {
    int id = 0;
    QString name;
    QString inputType = QStringLiteral("text");
    bool mutableValue = true;
    QString defaultValue;
    QStringList values;
    QJsonObject raw;

    static LabelAttribute fromJson(const QJsonObject& json);
};

struct Label {
    int id = 0;
    QString name;
    QString color = QStringLiteral("#b3b3b3");
    QString type = QStringLiteral("any");
    QVector<LabelAttribute> attributes;
    QVector<Label> sublabels;
    bool hasParent = false;
    QJsonObject raw;

    static Label fromJson(const QJsonObject& json);
    [[nodiscard]] QJsonObject toRequestJson(bool includeId = true) const;
};

struct FrameInfo {
    int width = 0;
    int height = 0;
    QString name;
    int relatedFiles = 0;

    static FrameInfo fromJson(const QJsonObject& json);
};

struct DataMeta {
    int size = 0;
    int startFrame = 0;
    int stopFrame = 0;
    int chunkSize = 0;
    int imageQuality = 0;
    QString chunkType;
    QString frameFilter;
    QDateTime chunksUpdatedDate;
    QVector<FrameInfo> frames;
    QVector<int> deletedFrames;
    QVector<int> includedFrames;
    QJsonObject raw;

    static DataMeta fromJson(const QJsonObject& json);
    [[nodiscard]] bool isDeleted(int frame) const;
};

struct RequestState {
    QString id;
    QString status;
    double progress = 0.0;
    QString message;
    QString resultUrl;
    int resultId = 0;
    QJsonObject operation;
    QJsonObject raw;

    [[nodiscard]] bool finished() const { return status == QStringLiteral("finished"); }
    [[nodiscard]] bool failed() const { return status == QStringLiteral("failed"); }

    static RequestState fromJson(const QJsonObject& json);
};

struct ServerInfo {
    QString name;
    QString description;
    QString version;
    QString logoUrl;
    QJsonObject raw;

    static ServerInfo fromJson(const QJsonObject& json);
};

struct FormatInfo {
    QString name;
    QString extension;
    QString version;
    bool enabled = false;
    QString dimension;

    static FormatInfo fromJson(const QJsonObject& json);
};

struct Formats {
    QVector<FormatInfo> importers;
    QVector<FormatInfo> exporters;

    static Formats fromJson(const QJsonObject& json);
};

int intValue(const QJsonObject& json, const char* key, int defaultValue = 0);
QString stringValue(const QJsonObject& json, const char* key, const QString& defaultValue = {});
bool boolValue(const QJsonObject& json, const char* key, bool defaultValue = false);

} // namespace cvat
