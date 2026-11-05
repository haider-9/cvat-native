#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/model/Annotation.h"

namespace cvat {

enum class PatchAction { Create, Update, Delete };

// Thin wrapper around /api/jobs/{id}/annotations. CVAT supports three save
// strategies: PUT replaces the whole job, PATCH applies an object-level
// create/update/delete operation, and DELETE clears annotations. The native
// client uses PATCH so local edits cannot silently discard remote work.
class AnnotationsApi {
public:
    explicit AnnotationsApi(ApiClient* client);

    RequestHandle* get(int jobId, std::function<void(Result<LabeledData>)> callback);
    RequestHandle* patch(int jobId, PatchAction action, const LabeledData& data,
                         std::function<void(Result<ApiResponse>)> callback);
    RequestHandle* put(int jobId, const LabeledData& data,
                       std::function<void(Result<ApiResponse>)> callback);
    RequestHandle* clear(int jobId, std::function<void(Result<ApiResponse>)> callback);

    RequestHandle* exportAnnotations(int jobId, const QString& format,
                                     std::function<void(Result<QByteArray>)> callback);
    RequestHandle* importAnnotations(int jobId, const QString& format,
                                     const QString& filePath,
                                     std::function<void(Result<ApiResponse>)> callback);

    static QString actionName(PatchAction action);

private:
    ApiClient* client_;
};

} // namespace cvat
